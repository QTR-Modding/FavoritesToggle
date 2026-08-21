// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../Internal.h"

#include <REL/ASM.h>
#include <REL/Pattern.h>
#include <REL/Trampoline.h>
#include <REX/FModule.h>

#include <array>
#include <atomic>
#include <limits>
#include <string_view>

namespace FavoritesToggle
{
    namespace
    {
        using Value = RE::Scaleform::GFx::Value;

        struct FmeV500
        {
            static constexpr std::string_view kModuleName =
                "FavoritesMenuExtended.dll";
            static constexpr std::uintptr_t kUseFavoriteRva = 0x25730;
            static constexpr std::array<std::uintptr_t, 4> kCallRvas{
                0x28871,
                0x28B3D,
                0x2B7A9,
                0x2BA7D
            };
        };

        constexpr std::ptrdiff_t kUseActionSpellCallOffset = 0x363;
        constexpr std::ptrdiff_t kPowerFeedbackCallOffset = 0x47F;

        using UseFavorite_t = void (*)(RE::TESBoundObject*);
        using UseActionSpell_t = void (*)(
            RE::Actor*,
            RE::SpellItem*);
        using PowerFeedback_t = bool (*)(RE::SpellItem*);

        REL::Relocation<UseFavorite_t> originalUseFavorite;
        REL::Trampoline trampoline{
            std::string_view{ "FavoritesMenuExtended" }
        };
        std::atomic_bool enabled{ false };
        UseActionSpell_t useActionSpell = nullptr;
        PowerFeedback_t playPowerFeedback = nullptr;

        bool Contains(
            const REX::FModuleSection& a_section,
            std::uintptr_t a_address,
            std::size_t a_size) noexcept
        {
            const auto begin = a_section.GetAddress();
            if (!begin || a_address < begin) {
                return false;
            }

            const auto offset = a_address - begin;
            return offset <= a_section.GetSize() &&
                a_size <= a_section.GetSize() - offset;
        }

        bool CanReachCall5(
            std::uintptr_t a_callsite,
            std::uintptr_t a_target) noexcept
        {
            const auto next = a_callsite + sizeof(REL::ASM::CALL5);
            if (a_target >= next) {
                return a_target - next <=
                    static_cast<std::uintptr_t>(
                        (std::numeric_limits<std::int32_t>::max)());
            }

            return next - a_target <=
                static_cast<std::uintptr_t>(
                    (std::numeric_limits<std::int32_t>::max)()) + 1;
        }

        RE::SpellItem* GetQuantumEssence()
        {
            static RE::SpellItem* spell = nullptr;
            if (!spell) {
                const auto* defaultObject =
                    RE::TESForm::LookupByEditorID<RE::BGSDefaultObject>(
                        "QuantumEssenceSpell_DO");
                spell = defaultObject && defaultObject->object ?
                    defaultObject->object->As<RE::SpellItem>() :
                    nullptr;
            }
            return spell;
        }

        bool ValidatePowerHelpers()
        {
            const REL::Relocation<std::uintptr_t> useQuickkey{
                RE::ID::FavoritesManager::UseQuickkey
            };
            const auto actionCall =
                useQuickkey.address() + kUseActionSpellCallOffset;
            const auto feedbackCall =
                useQuickkey.address() + kPowerFeedbackCallOffset;
            if (*reinterpret_cast<const std::uint8_t*>(actionCall) != 0xE8 ||
                *reinterpret_cast<const std::uint8_t*>(feedbackCall) != 0xE8) {
                return false;
            }

            const auto action = REL::ASM::CALL5::TARGET(actionCall);
            const auto feedback = REL::ASM::CALL5::TARGET(feedbackCall);
            if (!REL::Pattern<
                    "48 85 D2 0F 84 93 00 00 00 48 89 5C 24 08">()
                    .match(action) ||
                !REL::Pattern<
                    "48 89 5C 24 18 55 56 57 48 8B EC 48 83 EC 40">()
                    .match(feedback)) {
                return false;
            }

            useActionSpell = reinterpret_cast<UseActionSpell_t>(action);
            playPowerFeedback =
                reinterpret_cast<PowerFeedback_t>(feedback);
            return true;
        }

        bool TryHandlePower(
            RE::TESBoundObject* a_form,
            RE::PlayerCharacter* a_player)
        {
            auto* spell = a_form ? a_form->As<RE::SpellItem>() : nullptr;
            if (!spell || !a_player) {
                return false;
            }

            if (spell == GetQuantumEssence()) {
                useActionSpell(a_player, spell);
                playPowerFeedback(spell);
                return true;
            }

            if (!Power::IsSelected(spell, a_player)) {
                return false;
            }

            a_player->UnequipSpell(
                spell,
                RE::MagicSystem::CastingSource::kOther);
            playPowerFeedback(spell);
            return true;
        }

        void UseFavorite(RE::TESBoundObject* a_form) noexcept
        {
            if (enabled.load(std::memory_order_acquire)) {
                try {
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (TryHandlePower(a_form, player) ||
                        (a_form && !a_form->Is<RE::SpellItem>() &&
                            Toggle::TryUnequip(a_form, player))) {
                        return;
                    }
                } catch (...) {
                    REX::ERROR(
                        "Favorites Menu Extended toggle failed; "
                        "preserving its original behavior");
                }
            }

            originalUseFavorite(a_form);
        }

        class FavoritesUseListener final :
            public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                auto* root = a_params.movie &&
                        a_params.movie->asMovieRoot ?
                    a_params.movie->asMovieRoot.get() :
                    nullptr;
                if (!root) {
                    return;
                }

                Value menu;
                Value selectedIndex;
                if (root->GetVariable(&menu, "root1.Menu_mc") &&
                    menu.IsObject() &&
                    menu.GetMember("selectedIndex", &selectedIndex)) {
                    menu.SetMember(
                        "iLastSelectedQuickKey",
                        selectedIndex);
                }
            }
        };

        // Scaleform owns references to the function. Retain the native
        // handler for the process lifetime.
        auto* listener = new FavoritesUseListener{};

        void OnMenuMovieCreated(RE::IMenu* a_menu)
        {
            const auto* name = a_menu ? a_menu->GetName() : nullptr;
            if (!name ||
                std::string_view{ name } != "FavoritesMenu" ||
                !a_menu->uiMovie ||
                !a_menu->uiMovie->asMovieRoot) {
                return;
            }

            auto* root = a_menu->uiMovie->asMovieRoot.get();
            Value menu;
            Value loaderInfo;
            Value domain;
            Value manager;
            if (!root->GetVariable(&menu, a_menu->GetRootPath()) ||
                !menu.IsObject() ||
                !menu.GetMember("loaderInfo", &loaderInfo) ||
                !loaderInfo.IsObject() ||
                !loaderInfo.GetMember("applicationDomain", &domain) ||
                !domain.IsObject()) {
                return;
            }

            Value className;
            root->CreateString(
                &className,
                "Shared.AS3.Data::BSUIDataManager");
            if (!domain.Invoke(
                    "getDefinition",
                    &manager,
                    &className,
                    1) ||
                !manager.IsObject()) {
                return;
            }

            Value callback;
            Value eventName;
            root->CreateFunction(&callback, listener);
            root->CreateString(
                &eventName,
                "FavoritesMenu_UseQuickkey");
            Value args[2]{ eventName, callback };
            manager.Invoke("addEventListener", nullptr, args, 2);
        }
    }

    bool InstallFavoritesMenuExtended(
        const SFSE::MenuInterface* a_menuInterface)
    {
        const auto module =
            REX::FModule::GetLoadedModule(FmeV500::kModuleName);
        const auto base = module.GetBaseAddress();
        if (!base) {
            return true;
        }

        const auto version = REL::GetFileVersion(module.GetFileName());
        if (!version ||
            *version != REL::Version{ 5, 0, 0, 0 } ||
            !a_menuInterface ||
            !ValidatePowerHelpers()) {
            REX::ERROR(
                "Favorites Menu Extended compatibility requires version "
                "5.0.0");
            return false;
        }

        const auto text = module.GetSection(".text");
        const auto originalTarget = base + FmeV500::kUseFavoriteRva;
        if (!Contains(text, originalTarget, 22) ||
            !REL::Pattern<
                "48 89 5C 24 10 48 89 6C 24 18 56 57 "
                "41 54 41 56 41 57 48 83 EC 70">()
                .match(originalTarget)) {
            REX::ERROR(
                "Favorites Menu Extended use-favorite signature mismatch");
            return false;
        }

        for (const auto rva : FmeV500::kCallRvas) {
            const auto callsite = base + rva;
            if (!Contains(text, callsite, sizeof(REL::ASM::CALL5)) ||
                *reinterpret_cast<const std::uint8_t*>(callsite) != 0xE8 ||
                REL::ASM::CALL5::TARGET(callsite) != originalTarget) {
                REX::ERROR(
                    "Favorites Menu Extended callsite mismatch at "
                    "RVA 0x{:X}",
                    rva);
                return false;
            }
        }

        originalUseFavorite = originalTarget;
        const auto anchor = base +
            FmeV500::kCallRvas[FmeV500::kCallRvas.size() / 2];
        trampoline.create(
            sizeof(REL::ASM::JMP14),
            reinterpret_cast<void*>(anchor));

        const auto relay = trampoline.allocate_branch5(
            reinterpret_cast<std::uintptr_t>(&UseFavorite));
        for (const auto rva : FmeV500::kCallRvas) {
            if (!CanReachCall5(base + rva, relay)) {
                REX::ERROR(
                    "Favorites Menu Extended trampoline is unreachable");
                return false;
            }
        }

        for (const auto rva : FmeV500::kCallRvas) {
            const auto callsite = base + rva;
            const auto original = trampoline.write_call<5>(
                callsite,
                &UseFavorite);
            if (original != originalTarget ||
                REL::ASM::CALL5::TARGET(callsite) != relay) {
                REX::ERROR(
                    "Favorites Menu Extended hook failed at RVA 0x{:X}",
                    rva);
                return false;
            }
        }

        a_menuInterface->Register(OnMenuMovieCreated);
        enabled.store(true, std::memory_order_release);
        REX::INFO(
            "Enabled Favorites Menu Extended 5.0 compatibility");
        return true;
    }
}

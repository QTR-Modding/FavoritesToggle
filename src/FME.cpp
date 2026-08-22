// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PCH.h"
#include "Hooks.h"

#include <REL/Trampoline.h>
#include <REX/FModule.h>

#include <array>
#include <atomic>
#include <limits>
#include <string_view>

namespace FavoritesToggle::FME
{
    namespace
    {
        using Value = RE::Scaleform::GFx::Value;
        using UseFavoriteFn = void (*)(RE::TESBoundObject*);
        using UseActionSpell = void (*)(RE::Actor*, RE::SpellItem*);
        using PowerFeedback = bool (*)(RE::SpellItem*);

        constexpr std::string_view kModuleName = "FavoritesMenuExtended.dll";
        constexpr std::uintptr_t kUseFavoriteRva = 0x25730;
        constexpr std::array<std::uintptr_t, 4> kCallRvas{ 0x28871, 0x28B3D, 0x2B7A9, 0x2BA7D };
        constexpr std::ptrdiff_t kUseActionSpellCallOffset = 0x363;
        constexpr std::ptrdiff_t kPowerFeedbackCallOffset = 0x47F;
        constexpr auto kPowerSource = RE::MagicSystem::CastingSource::kOther;

        REL::Relocation<UseFavoriteFn> originalUseFavorite;
        REL::Relocation<UseActionSpell> useActionSpell;
        REL::Relocation<PowerFeedback> playPowerFeedback;
        REL::Trampoline trampoline{ "FavoritesMenuExtended" };
        std::atomic_bool enabled{ false };

        [[nodiscard]] bool Contains(
            const REX::FModuleSection& a_section,
            std::uintptr_t a_address,
            std::size_t a_size) noexcept
        {
            const auto begin = a_section.GetAddress();
            if (!begin || a_address < begin) {
                return false;
            }

            const auto offset = a_address - begin;
            return offset <= a_section.GetSize() && a_size <= a_section.GetSize() - offset;
        }

        [[nodiscard]] bool CanReachCall5(std::uintptr_t a_callsite, std::uintptr_t a_target) noexcept
        {
            const auto next = a_callsite + sizeof(REL::ASM::CALL5);
            if (a_target >= next) {
                return a_target - next <=
                    static_cast<std::uintptr_t>((std::numeric_limits<std::int32_t>::max)());
            }
            return next - a_target <=
                static_cast<std::uintptr_t>((std::numeric_limits<std::int32_t>::max)()) + 1;
        }

        [[nodiscard]] RE::SpellItem* GetQuantumEssence()
        {
            static RE::SpellItem* spell = nullptr;
            if (!spell) {
                const auto* defaultObject =
                    RE::TESForm::LookupByEditorID<RE::BGSDefaultObject>("QuantumEssenceSpell_DO");
                spell = defaultObject && defaultObject->object ?
                    defaultObject->object->As<RE::SpellItem>() : nullptr;
            }
            return spell;
        }

        [[nodiscard]] bool ValidatePowerHelpers()
        {
            const REL::Relocation<std::uintptr_t> useQuickkey{ RE::ID::FavoritesManager::UseQuickkey };
            const auto actionCall = useQuickkey.address() + kUseActionSpellCallOffset;
            const auto feedbackCall = useQuickkey.address() + kPowerFeedbackCallOffset;
            if (*reinterpret_cast<const std::uint8_t*>(actionCall) != 0xE8 ||
                *reinterpret_cast<const std::uint8_t*>(feedbackCall) != 0xE8) {
                return false;
            }

            const auto action = REL::ASM::CALL5::TARGET(actionCall);
            const auto feedback = REL::ASM::CALL5::TARGET(feedbackCall);
            if (!REL::Pattern<"48 85 D2 0F 84 93 00 00 00 48 89 5C 24 08">().match(action) ||
                !REL::Pattern<"48 89 5C 24 18 55 56 57 48 8B EC 48 83 EC 40">().match(feedback)) {
                return false;
            }

            useActionSpell = action;
            playPowerFeedback = feedback;
            return true;
        }

        [[nodiscard]] bool TryUnequip(RE::TESBoundObject& a_form, RE::PlayerCharacter& a_player)
        {
            RE::BSTSmartPointer<RE::TBO_InstanceData> instanceData;
            bool found = false;
            bool locked = false;
            a_player.ForEachEquippedItem([&](const RE::BGSInventoryItem& a_item) {
                if (a_item.object != &a_form) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                found = true;
                instanceData = a_item.instanceData;
                locked = a_item.flags.any(RE::BGSInventoryItem::Flag::kEquipStateLocked);
                return RE::BSContainer::ForEachResult::kStop;
            });

            if (!found) {
                return false;
            }
            if (locked) {
                return true;
            }

            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) {
                return false;
            }

            const RE::BGSObjectInstance instance(&a_form, instanceData.get());
            equipManager->UnequipObject(&a_player, instance, nullptr, true, false, true, false, nullptr);
            return true;
        }

        [[nodiscard]] bool TryHandlePower(RE::SpellItem& a_spell, RE::PlayerCharacter& a_player)
        {
            if (&a_spell == GetQuantumEssence()) {
                useActionSpell(&a_player, &a_spell);
                playPowerFeedback(&a_spell);
                return true;
            }
            if (a_player.GetEquippedSpell(kPowerSource) != &a_spell) {
                return false;
            }

            a_player.UnequipSpell(&a_spell, kPowerSource);
            playPowerFeedback(&a_spell);
            return true;
        }

        void UseFavorite(RE::TESBoundObject* a_form) noexcept
        {
            if (enabled.load(std::memory_order_acquire) && a_form) {
                try {
                    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                        auto* spell = a_form->As<RE::SpellItem>();
                        if (spell ? TryHandlePower(*spell, *player) : TryUnequip(*a_form, *player)) {
                            return;
                        }
                    }
                } catch (...) {
                    logger::error(
                        "Favorites Menu Extended toggle failed; preserving its original behavior");
                }
            }
            originalUseFavorite(a_form);
        }

        class FavoritesUseListener final : public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                try {
                    auto* root = a_params.movie ? a_params.movie->asMovieRoot.get() : nullptr;
                    if (!root) {
                        return;
                    }

                    Value menu;
                    Value selectedIndex;
                    if (root->GetVariable(&menu, "root1.Menu_mc") && menu.IsObject() &&
                        menu.GetMember("selectedIndex", &selectedIndex)) {
                        menu.SetMember("iLastSelectedQuickKey", selectedIndex);
                    }
                } catch (...) {
                    logger::error("Favorites Menu Extended menu callback failed");
                }
            }
        };

        // Scaleform owns references to the function. Retain the native handler
        // for the process lifetime.
        auto* const listener = new FavoritesUseListener{};

        void OnMenuMovieCreated(RE::IMenu* a_menu) noexcept
        {
            try {
                const auto* name = a_menu ? a_menu->GetName() : nullptr;
                if (!name || std::string_view{ name } != "FavoritesMenu" || !a_menu->uiMovie ||
                    !a_menu->uiMovie->asMovieRoot) {
                    return;
                }

                auto* root = a_menu->uiMovie->asMovieRoot.get();
                Value menu;
                Value loaderInfo;
                Value domain;
                Value manager;
                if (!root->GetVariable(&menu, a_menu->GetRootPath()) || !menu.IsObject() ||
                    !menu.GetMember("loaderInfo", &loaderInfo) || !loaderInfo.IsObject() ||
                    !loaderInfo.GetMember("applicationDomain", &domain) || !domain.IsObject()) {
                    return;
                }

                Value className;
                root->CreateString(&className, "Shared.AS3.Data::BSUIDataManager");
                if (!domain.Invoke("getDefinition", &manager, &className, 1) || !manager.IsObject()) {
                    return;
                }

                Value callback;
                Value eventName;
                root->CreateFunction(&callback, listener);
                root->CreateString(&eventName, "FavoritesMenu_UseQuickkey");
                Value args[2]{ eventName, callback };
                if (!manager.Invoke("addEventListener", nullptr, args, 2)) {
                    logger::warn("Favorites Menu Extended input bridge was not registered");
                }
            } catch (...) {
                logger::error("Favorites Menu Extended menu setup failed");
            }
        }
    }

    bool Install()
    {
        const auto module = REX::FModule::GetLoadedModule(kModuleName);
        const auto base = module.GetBaseAddress();
        if (!base) {
            return true;
        }

        const auto version = REL::GetFileVersion(module.GetFileName());
        if (!version || *version != REL::Version{ 5, 0, 0, 0 }) {
            logger::error("Favorites Menu Extended compatibility requires version 5.0.0");
            return false;
        }
        const auto* menuInterface = SFSE::GetMenuInterface();
        if (!menuInterface) {
            logger::error("SFSE menu interface is unavailable");
            return false;
        }
        if (!ValidatePowerHelpers()) {
            logger::error("Favorites Menu Extended Power helper signatures do not match");
            return false;
        }

        const auto text = module.GetSection(".text");
        const auto originalTarget = base + kUseFavoriteRva;
        if (!Contains(text, originalTarget, 22) ||
            !REL::Pattern<
                "48 89 5C 24 10 48 89 6C 24 18 56 57 41 54 41 56 41 57 48 83 EC 70">()
                .match(originalTarget)) {
            logger::error("Favorites Menu Extended use-favorite signature mismatch");
            return false;
        }

        for (const auto rva : kCallRvas) {
            const auto callsite = base + rva;
            if (!Contains(text, callsite, sizeof(REL::ASM::CALL5)) ||
                *reinterpret_cast<const std::uint8_t*>(callsite) != 0xE8 ||
                REL::ASM::CALL5::TARGET(callsite) != originalTarget) {
                logger::error("Favorites Menu Extended callsite mismatch at RVA 0x{:X}", rva);
                return false;
            }
        }

        originalUseFavorite = originalTarget;
        const auto anchor = base + kCallRvas[kCallRvas.size() / 2];
        trampoline.create(sizeof(REL::ASM::JMP14), reinterpret_cast<void*>(anchor));

        const auto relay = trampoline.allocate_branch5(reinterpret_cast<std::uintptr_t>(&UseFavorite));
        for (const auto rva : kCallRvas) {
            if (!CanReachCall5(base + rva, relay)) {
                logger::error("Favorites Menu Extended trampoline is unreachable");
                return false;
            }
        }

        for (const auto rva : kCallRvas) {
            const auto callsite = base + rva;
            const auto original = trampoline.write_call<5>(callsite, &UseFavorite);
            if (original != originalTarget || REL::ASM::CALL5::TARGET(callsite) != relay) {
                logger::error("Favorites Menu Extended hook failed at RVA 0x{:X}", rva);
                return false;
            }
        }

        menuInterface->Register(OnMenuMovieCreated);
        enabled.store(true, std::memory_order_release);
        logger::info("Enabled Favorites Menu Extended 5.0 compatibility");
        return true;
    }
}

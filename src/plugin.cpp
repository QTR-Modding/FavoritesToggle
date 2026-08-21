// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PCH.h"

namespace FavoritesToggle
{
    namespace
    {
        constexpr auto kPowerSource = RE::MagicSystem::CastingSource::kOther;

        [[nodiscard]] bool IsSelectedPower(
            RE::SpellItem* a_spell,
            RE::Actor* a_actor)
        {
            return a_spell &&
                a_actor &&
                a_actor->GetEquippedSpell(kPowerSource) == a_spell;
        }
    }

    namespace Vanilla
    {
        namespace
        {
            constexpr std::ptrdiff_t kInventorySignatureOffset = 0x2D5;
            constexpr std::ptrdiff_t kAllowUnequipOffset = 0x2E3;
            constexpr std::ptrdiff_t kSpellStateCallOffset = 0x377;

            using SpellStateHelper = bool (*)(
                void*,
                RE::Actor*,
                RE::SpellItem*);
            REL::Relocation<SpellStateHelper> originalSpellState;

            bool FilterSpellState(
                void* a_context,
                RE::Actor* a_actor,
                RE::SpellItem* a_spell) noexcept
            {
                try {
                    if (IsSelectedPower(a_spell, a_actor)) {
                        return false;
                    }
                } catch (...) {
                    logger::error(
                        "Selected-Power check failed; preserving the "
                        "original spell-state result");
                }

                return originalSpellState(a_context, a_actor, a_spell);
            }

            bool InstallInventoryToggle()
            {
                const REL::Relocation<std::uintptr_t> useQuickkey{
                    RE::ID::FavoritesManager::UseQuickkey
                };
                const auto signature =
                    useQuickkey.address() + kInventorySignatureOffset;
                if (!REL::Pattern<
                        "C6 44 24 30 01 C6 44 24 28 00 "
                        "C6 44 24 20 ?? 4C 8D">().match(signature)) {
                    logger::error(
                        "Vanilla inventory toggle signature mismatch");
                    return false;
                }

                const auto address =
                    useQuickkey.address() + kAllowUnequipOffset;
                const auto current =
                    *reinterpret_cast<const std::uint8_t*>(address);
                if (current == 1) {
                    return true;
                }
                if (current != 0) {
                    logger::error(
                        "Unexpected vanilla allow-unequip value {}",
                        current);
                    return false;
                }

                constexpr std::uint8_t enabled = 1;
                const bool wrote = REL::WriteSafeData(address, enabled);
                const bool verified = wrote &&
                    *reinterpret_cast<const std::uint8_t*>(address) == enabled;
                if (!verified) {
                    const bool restored =
                        REL::WriteSafeData(address, current) &&
                        *reinterpret_cast<const std::uint8_t*>(address) == current;
                    logger::error(
                        "Vanilla inventory toggle failed; rollback={}",
                        restored);
                    return false;
                }

                logger::info("Enabled vanilla inventory favorite toggling");
                return true;
            }

            bool InstallPowerToggle()
            {
                REL::Relocation<std::uintptr_t> call{
                    RE::ID::FavoritesManager::UseQuickkey,
                    kSpellStateCallOffset
                };
                originalSpellState = call.write_call<5>(&FilterSpellState);

                logger::info("Enabled vanilla selected-Power toggling");
                return true;
            }
        }

        [[nodiscard]] bool Install()
        {
            const REL::Relocation<std::uintptr_t> useQuickkey{
                RE::ID::FavoritesManager::UseQuickkey
            };
            const auto callsite =
                useQuickkey.address() + kSpellStateCallOffset;
            const auto spellState = REL::ASM::CALL5::TARGET(callsite);
            if (!REL::Pattern<
                    "4C 8B C3 48 8B 15 39 3B 08 04 "
                    "E8 ?? ?? ?? ?? 84 C0 0F 85 F8 00 00 00">()
                    .match(callsite - 10) ||
                !REL::Pattern<
                    "48 89 5C 24 10 48 89 6C 24 18 "
                    "48 89 74 24 20 48 89 4C 24 08">()
                    .match(spellState)) {
                logger::error("Vanilla Power hook signature mismatch");
                return false;
            }

            return InstallInventoryToggle() && InstallPowerToggle();
        }
    }

    namespace FME
    {
        namespace
        {
            using Value = RE::Scaleform::GFx::Value;

            constexpr std::string_view kModuleName =
                "FavoritesMenuExtended.dll";
            constexpr std::uintptr_t kUseFavoriteRva = 0x25730;
            constexpr std::array<std::uintptr_t, 4> kCallRvas{
                0x28871,
                0x28B3D,
                0x2B7A9,
                0x2BA7D
            };
            constexpr std::ptrdiff_t kUseActionSpellCallOffset = 0x363;
            constexpr std::ptrdiff_t kPowerFeedbackCallOffset = 0x47F;

            using UseFavoriteFn = void (*)(RE::TESBoundObject*);
            using UseActionSpell = void (*)(RE::Actor*, RE::SpellItem*);
            using PowerFeedback = bool (*)(RE::SpellItem*);

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
                return offset <= a_section.GetSize() &&
                    a_size <= a_section.GetSize() - offset;
            }

            [[nodiscard]] bool CanReachCall5(
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

            [[nodiscard]] RE::SpellItem* GetQuantumEssence()
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

            [[nodiscard]] bool ValidatePowerHelpers()
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

                useActionSpell = action;
                playPowerFeedback = feedback;
                return true;
            }

            [[nodiscard]] bool TryUnequip(
                RE::TESBoundObject* a_form,
                RE::PlayerCharacter* a_player)
            {
                if (!a_form || !a_player) {
                    return false;
                }

                RE::TESBoundObject* equippedObject = nullptr;
                RE::BSTSmartPointer<RE::TBO_InstanceData> instanceData;
                bool locked = false;
                a_player->ForEachEquippedItem(
                    [&](const RE::BGSInventoryItem& a_item) {
                        if (a_item.object != a_form) {
                            return RE::BSContainer::ForEachResult::kContinue;
                        }

                        equippedObject = a_item.object;
                        instanceData = a_item.instanceData;
                        locked = a_item.flags.any(
                            RE::BGSInventoryItem::Flag::kEquipStateLocked);
                        return RE::BSContainer::ForEachResult::kStop;
                    });

                if (!equippedObject) {
                    return false;
                }
                if (locked) {
                    return true;
                }

                auto* equipManager = RE::ActorEquipManager::GetSingleton();
                if (!equipManager) {
                    return false;
                }

                const RE::BGSObjectInstance instance(
                    equippedObject,
                    instanceData.get());
                equipManager->UnequipObject(
                    a_player,
                    instance,
                    nullptr,
                    true,
                    false,
                    true,
                    false,
                    nullptr);
                return true;
            }

            [[nodiscard]] bool TryHandlePower(
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

                if (!IsSelectedPower(spell, a_player)) {
                    return false;
                }

                a_player->UnequipSpell(spell, kPowerSource);
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
                                TryUnequip(a_form, player))) {
                            return;
                        }
                    } catch (...) {
                        logger::error(
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
                    try {
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
                    } catch (...) {
                        logger::error(
                            "Favorites Menu Extended menu callback failed");
                    }
                }
            };

            // Scaleform owns references to the function. Retain the native
            // handler for the process lifetime.
            auto* const listener = new FavoritesUseListener{};

            void OnMenuMovieCreated(RE::IMenu* a_menu) noexcept
            {
                try {
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
                    if (!manager.Invoke(
                            "addEventListener",
                            nullptr,
                            args,
                            2)) {
                        logger::warn(
                            "Favorites Menu Extended input bridge was not "
                            "registered");
                    }
                } catch (...) {
                    logger::error(
                        "Favorites Menu Extended menu setup failed");
                }
            }
        }

        [[nodiscard]] bool Install(
            const SFSE::MenuInterface* a_menuInterface)
        {
            const auto module = REX::FModule::GetLoadedModule(kModuleName);
            const auto base = module.GetBaseAddress();
            if (!base) {
                return true;
            }

            const auto version = REL::GetFileVersion(module.GetFileName());
            if (!version || *version != REL::Version{ 5, 0, 0, 0 }) {
                logger::error(
                    "Favorites Menu Extended compatibility requires "
                    "version 5.0.0");
                return false;
            }
            if (!a_menuInterface) {
                logger::error("SFSE menu interface is unavailable");
                return false;
            }
            if (!ValidatePowerHelpers()) {
                logger::error(
                    "Favorites Menu Extended Power helper signatures "
                    "do not match");
                return false;
            }

            const auto text = module.GetSection(".text");
            const auto originalTarget = base + kUseFavoriteRva;
            if (!Contains(text, originalTarget, 22) ||
                !REL::Pattern<
                    "48 89 5C 24 10 48 89 6C 24 18 56 57 "
                    "41 54 41 56 41 57 48 83 EC 70">()
                    .match(originalTarget)) {
                logger::error(
                    "Favorites Menu Extended use-favorite signature "
                    "mismatch");
                return false;
            }

            for (const auto rva : kCallRvas) {
                const auto callsite = base + rva;
                if (!Contains(text, callsite, sizeof(REL::ASM::CALL5)) ||
                    *reinterpret_cast<const std::uint8_t*>(callsite) != 0xE8 ||
                    REL::ASM::CALL5::TARGET(callsite) != originalTarget) {
                    logger::error(
                        "Favorites Menu Extended callsite mismatch at "
                        "RVA 0x{:X}",
                        rva);
                    return false;
                }
            }

            originalUseFavorite = originalTarget;
            const auto anchor =
                base + kCallRvas[kCallRvas.size() / 2];
            trampoline.create(
                sizeof(REL::ASM::JMP14),
                reinterpret_cast<void*>(anchor));

            const auto relay = trampoline.allocate_branch5(
                reinterpret_cast<std::uintptr_t>(&UseFavorite));
            for (const auto rva : kCallRvas) {
                if (!CanReachCall5(base + rva, relay)) {
                    logger::error(
                        "Favorites Menu Extended trampoline is unreachable");
                    return false;
                }
            }

            for (const auto rva : kCallRvas) {
                const auto callsite = base + rva;
                const auto original = trampoline.write_call<5>(
                    callsite,
                    &UseFavorite);
                if (original != originalTarget ||
                    REL::ASM::CALL5::TARGET(callsite) != relay) {
                    logger::error(
                        "Favorites Menu Extended hook failed at RVA 0x{:X}",
                        rva);
                    return false;
                }
            }

            a_menuInterface->Register(OnMenuMovieCreated);
            enabled.store(true, std::memory_order_release);
            logger::info(
                "Enabled Favorites Menu Extended 5.0 compatibility");
            return true;
        }
    }

    void OnPostLoad(SFSE::MessagingInterface::Message* a_message) noexcept
    {
        if (!a_message ||
            a_message->type != SFSE::MessagingInterface::kPostLoad) {
            return;
        }

        try {
            if (!Vanilla::Install()) {
                logger::error("Favorites Toggle is inactive");
                return;
            }
        } catch (...) {
            logger::error("Favorites Toggle installation failed");
            return;
        }

        try {
            if (!FME::Install(SFSE::GetMenuInterface())) {
                logger::warn(
                    "Favorites Menu Extended compatibility is inactive");
            }
        } catch (...) {
            logger::warn(
                "Favorites Menu Extended compatibility failed to install");
        }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    SFSE::InitInfo initInfo{
        .logPattern = "%Y-%m-%d %H:%M:%S.%e [%l] %v",
        .trampoline = true,
        .trampolineSize = sizeof(REL::ASM::JMP14)
    };
    SFSE::Init(a_sfse, initInfo);

    const auto* messaging = SFSE::GetMessagingInterface();
    if (!messaging) {
        logger::error("SFSE messaging interface is unavailable");
        return false;
    }

    if (!messaging->RegisterListener(FavoritesToggle::OnPostLoad)) {
        logger::error("Could not register the SFSE message listener");
        return false;
    }

    logger::info(
        "Loaded for Starfield {}",
        a_sfse->RuntimeVersion().string("."));
    return true;
}

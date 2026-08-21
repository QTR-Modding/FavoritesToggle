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

    void OnPostLoad(SFSE::MessagingInterface::Message* a_message) noexcept
    {
        if (!a_message ||
            a_message->type != SFSE::MessagingInterface::kPostLoad) {
            return;
        }

        try {
            if (!Vanilla::Install()) {
                logger::error("Favorites Toggle is inactive");
            }
        } catch (...) {
            logger::error("Favorites Toggle installation failed");
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

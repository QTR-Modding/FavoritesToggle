// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

#include <REL/ASM.h>
#include <REL/Pattern.h>
#include <REL/Relocation.h>
#include <REL/Utility.h>

namespace FavoritesToggle
{
    namespace
    {
        constexpr REL::ID kUseQuickkeyID{ 113918 };
        constexpr std::uintptr_t kUseQuickkeyRva = 0x1EBF380;
        constexpr std::ptrdiff_t kInventorySignatureOffset = 0x2D5;
        constexpr std::ptrdiff_t kAllowUnequipOffset = 0x2E3;
        constexpr std::ptrdiff_t kSpellStateCallOffset = 0x377;

        using SpellStateHelper_t = bool (*)(
            void*,
            RE::Actor*,
            RE::SpellItem*);
        REL::Relocation<SpellStateHelper_t> originalSpellState;

        bool FilterSpellState(
            void* a_context,
            RE::Actor* a_actor,
            RE::SpellItem* a_spell) noexcept
        {
            try {
                if (Power::IsSelected(a_spell, a_actor)) {
                    return false;
                }
            } catch (...) {
                REX::ERROR(
                    "Selected-Power check failed; preserving the original "
                    "spell-state result");
            }

            return originalSpellState(a_context, a_actor, a_spell);
        }

        bool InstallInventoryToggle()
        {
            const REL::Relocation<std::uintptr_t> useQuickkey{
                kUseQuickkeyID
            };
            const auto signature =
                useQuickkey.address() + kInventorySignatureOffset;
            if (useQuickkey.offset() != kUseQuickkeyRva ||
                !REL::Pattern<
                    "C6 44 24 30 01 C6 44 24 28 00 "
                    "C6 44 24 20 ?? 4C 8D">().match(signature)) {
                REX::ERROR("Vanilla inventory toggle signature mismatch");
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
                REX::ERROR(
                    "Unexpected vanilla allow-unequip value {}",
                    current);
                return false;
            }

            constexpr std::uint8_t enabled = 1;
            const bool wrote = REL::WriteSafeData(address, enabled);
            const bool verified = wrote &&
                *reinterpret_cast<const std::uint8_t*>(address) == enabled;
            if (!verified) {
                const bool restored = REL::WriteSafeData(address, current) &&
                    *reinterpret_cast<const std::uint8_t*>(address) == current;
                REX::ERROR(
                    "Vanilla inventory toggle failed; rollback={}",
                    restored);
                return false;
            }

            REX::INFO("Enabled vanilla inventory favorite toggling");
            return true;
        }

        bool InstallPowerToggle()
        {
            REL::Relocation<std::uintptr_t> call{
                kUseQuickkeyID,
                kSpellStateCallOffset
            };
            originalSpellState = call.write_call<5>(&FilterSpellState);

            REX::INFO("Enabled vanilla selected-Power toggling");
            return true;
        }
    }

    bool InstallVanilla()
    {
        const REL::Relocation<std::uintptr_t> useQuickkey{
            kUseQuickkeyID
        };
        const REL::Relocation<std::uintptr_t> spellState{
            REL::ID(101955)
        };
        const auto callsite =
            useQuickkey.address() + kSpellStateCallOffset;
        if (useQuickkey.offset() != kUseQuickkeyRva ||
            spellState.offset() != 0x19A3BA0 ||
            !Power::ValidateSelected() ||
            !REL::Pattern<
                "4C 8B C3 48 8B 15 39 3B 08 04 "
                "E8 ?? ?? ?? ?? 84 C0 0F 85 F8 00 00 00">()
                .match(callsite - 10) ||
            REL::ASM::CALL5::TARGET(callsite) != spellState.address()) {
            REX::ERROR("Vanilla Power hook signature mismatch");
            return false;
        }

        return InstallInventoryToggle() && InstallPowerToggle();
    }
}

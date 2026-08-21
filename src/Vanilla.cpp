// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

#include <REL/Pattern.h>

#include <array>
#include <cstring>

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
                !REL::Pattern<
                    "4C 8B C3 48 8B 15 39 3B 08 04 "
                    "E8 ?? ?? ?? ?? 84 C0 0F 85 F8 00 00 00">()
                    .match(callsite - 10) ||
                REL::ASM::CALL5::TARGET(callsite) != spellState.address()) {
                REX::ERROR("Vanilla Power hook signature mismatch");
                return false;
            }

            originalSpellState = spellState.address();
            const auto wrapper =
                reinterpret_cast<std::uintptr_t>(&FilterSpellState);
            const auto stub =
                REL::GetTrampoline().allocate_branch5(wrapper);
            const auto* branch =
                reinterpret_cast<const REL::ASM::JMP14*>(stub);
            if (!stub ||
                !detail::FitsCall5(callsite, stub) ||
                branch->jmp.op != 0xFF ||
                branch->jmp.rm != 0x25 ||
                branch->jmp.disp != 0 ||
                branch->addr != wrapper) {
                REX::ERROR("Vanilla Power hook trampoline is invalid");
                return false;
            }

            constexpr auto size = sizeof(REL::ASM::CALL5);
            std::array<std::uint8_t, size> original{};
            std::memcpy(
                original.data(),
                reinterpret_cast<const void*>(callsite),
                size);

            const REL::ASM::CALL5 replacement{ callsite, stub };
            if (!REL::WriteSafeData(callsite, replacement) ||
                std::memcmp(
                    reinterpret_cast<const void*>(callsite),
                    &replacement,
                    size) != 0 ||
                REL::ASM::CALL5::TARGET(callsite) != stub) {
                const bool restored =
                    REL::WriteSafeData(callsite, original) &&
                    std::memcmp(
                        reinterpret_cast<const void*>(callsite),
                        original.data(),
                        size) == 0;
                REX::ERROR(
                    "Vanilla Power hook failed; rollback={}",
                    restored);
                return false;
            }

            REX::INFO("Enabled vanilla selected-Power toggling");
            return true;
        }
    }

    bool InstallVanilla()
    {
        const bool inventory = InstallInventoryToggle();
        const bool power =
            Power::ValidateSelected() && InstallPowerToggle();
        return inventory && power;
    }
}

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
        constexpr REL::ID kUseInventoryItemID{ 101954 };
        constexpr std::uintptr_t kUseQuickkeyRva = 0x1EBF380;
        constexpr std::uintptr_t kUseInventoryItemRva = 0x19A3330;
        constexpr std::ptrdiff_t kInventoryCallOffset = 0x2F6;
        constexpr std::ptrdiff_t kSpellStateCallOffset = 0x377;

        using UseInventoryItem_t = bool (*)(
            RE::ActorEquipManager*,
            RE::Actor*,
            const RE::InventoryInterface::Handle*,
            std::uint8_t,
            bool,
            bool,
            bool);
        using SpellStateHelper_t = bool (*)(
            void*,
            RE::Actor*,
            RE::SpellItem*);
        REL::Relocation<UseInventoryItem_t> originalUseInventoryItem;
        REL::Relocation<SpellStateHelper_t> originalSpellState;

        bool UseInventoryItem(
            RE::ActorEquipManager* a_equipManager,
            RE::Actor* a_actor,
            const RE::InventoryInterface::Handle* a_handle,
            std::uint8_t a_result,
            bool a_allowUnequip,
            bool a_flag6,
            bool a_flag7) noexcept
        {
            try {
                auto* inventory = RE::BGSInventoryInterface::GetSingleton();
                if (inventory && a_handle) {
                    if (auto* form = inventory->GetInventoryObject(
                            a_handle->id)) {
                        a_allowUnequip = Settings::Allows(form);
                    }
                }
            } catch (...) {
                REX::ERROR(
                    "Favorite type check failed; preserving vanilla behavior");
            }

            return originalUseInventoryItem(
                a_equipManager,
                a_actor,
                a_handle,
                a_result,
                a_allowUnequip,
                a_flag6,
                a_flag7);
        }

        bool FilterSpellState(
            void* a_context,
            RE::Actor* a_actor,
            RE::SpellItem* a_spell) noexcept
        {
            try {
                if (Settings::Powers() &&
                    Power::IsSelected(a_spell, a_actor)) {
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
            const REL::Relocation<std::uintptr_t> useInventoryItem{
                kUseInventoryItemID
            };
            REL::Relocation<std::uintptr_t> call{
                kUseQuickkeyID,
                kInventoryCallOffset
            };
            if (useQuickkey.offset() != kUseQuickkeyRva ||
                useInventoryItem.offset() != kUseInventoryItemRva ||
                !REL::Pattern<
                    "C6 44 24 30 01 C6 44 24 28 00 C6 44 24 20 00 "
                    "4C 8D 45 67 48 8B 15 C1 3B 08 04 "
                    "48 8B 0D CA 2E 32 04 E8 ?? ?? ?? ?? 0F B6 F8">()
                    .match(call.address() - 33) ||
                REL::ASM::CALL5::TARGET(call.address()) !=
                    useInventoryItem.address()) {
                REX::ERROR("Vanilla inventory hook signature mismatch");
                return false;
            }

            originalUseInventoryItem =
                call.write_call<5>(&UseInventoryItem);

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

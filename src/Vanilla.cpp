// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PCH.h"
#include "Hooks.h"

namespace FavoritesToggle::Vanilla
{
    namespace
    {
        constexpr std::ptrdiff_t kInventoryCallOffset = 0x2F6;
        constexpr std::ptrdiff_t kSpellStateCallOffset = 0x377;

        using UseInventoryItemFn = bool (*)(
            RE::ActorEquipManager*,
            RE::Actor*,
            const RE::InventoryInterface::Handle*,
            std::uint8_t,
            bool,
            bool,
            bool);
        using SpellStateHelper = bool (*)(
            void*,
            RE::Actor*,
            RE::SpellItem*);

        REL::Relocation<UseInventoryItemFn> originalUseInventoryItem;
        REL::Relocation<SpellStateHelper> originalSpellState;

        [[nodiscard]] bool IsSelectedPower(
            RE::SpellItem* a_spell,
            RE::Actor* a_actor)
        {
            return a_spell &&
                a_actor &&
                a_actor->GetEquippedSpell(
                    RE::MagicSystem::CastingSource::kOther) == a_spell;
        }

        bool UseInventoryItem(
            RE::ActorEquipManager* a_equipManager,
            RE::Actor* a_actor,
            const RE::InventoryInterface::Handle* a_handle,
            std::uint8_t a_result,
            bool a_allowUnequip,
            bool a_unk5,
            bool a_unk6) noexcept
        {
            auto* inventory = RE::BGSInventoryInterface::GetSingleton();
            if (inventory && a_handle) {
                if (const auto* form =
                        inventory->GetInventoryObject(a_handle->id)) {
                    a_allowUnequip = Settings::Allows(*form);
                }
            }

            return originalUseInventoryItem(
                a_equipManager,
                a_actor,
                a_handle,
                a_result,
                a_allowUnequip,
                a_unk5,
                a_unk6);
        }

        bool FilterSpellState(
            void* a_context,
            RE::Actor* a_actor,
            RE::SpellItem* a_spell) noexcept
        {
            try {
                if (Settings::Powers() &&
                    IsSelectedPower(a_spell, a_actor)) {
                    return false;
                }
            } catch (...) {
                logger::error(
                    "Selected-Power check failed; preserving the "
                    "original spell-state result");
            }

            return originalSpellState(a_context, a_actor, a_spell);
        }

        [[nodiscard]] bool InstallInventoryToggle()
        {
            const REL::Relocation<std::uintptr_t> useInventoryItem{
                RE::ID::ActorEquipManager::UseInventoryItem
            };
            REL::Relocation<std::uintptr_t> call{
                RE::ID::FavoritesManager::UseQuickkey,
                kInventoryCallOffset
            };
            if (!REL::Pattern<
                    "C6 44 24 30 01 C6 44 24 28 00 C6 44 24 20 00 "
                    "4C 8D 45 67 48 8B 15 C1 3B 08 04 "
                    "48 8B 0D CA 2E 32 04 E8 ?? ?? ?? ?? 0F B6 F8">()
                    .match(call.address() - 33) ||
                REL::ASM::CALL5::TARGET(call.address()) !=
                    useInventoryItem.address()) {
                logger::error("Vanilla inventory hook signature mismatch");
                return false;
            }

            originalUseInventoryItem =
                call.write_call<5>(&UseInventoryItem);

            logger::info("Enabled vanilla inventory favorite toggling");
            return true;
        }

        [[nodiscard]] bool InstallPowerToggle()
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

    bool Install()
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

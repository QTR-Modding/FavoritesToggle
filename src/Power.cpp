// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

#include <REL/Pattern.h>

namespace FavoritesToggle::Power
{
    namespace
    {
        constexpr std::uint32_t kPowerSlot = 2;

        using UnequipSpell_t = void (*)(
            RE::ActorEquipManager*,
            RE::Actor*,
            RE::SpellItem*,
            const RE::BGSEquipSlot*);
        using GetSelectedPower_t = RE::SpellItem* (*)(
            void*,
            std::uint32_t,
            RE::Actor*,
            std::uint32_t);
        using GetPowerEquipSlot_t = const RE::BGSEquipSlot* (*)(
            std::uint8_t);
        using UseActionSpell_t = void (*)(
            RE::Actor*,
            RE::SpellItem*);
        using PostUseSpell_t = bool (*)(RE::SpellItem*);

        auto& UnequipSpell()
        {
            static REL::Relocation<UnequipSpell_t> value{ REL::ID(101952) };
            return value;
        }

        auto& GetSelectedPower()
        {
            static REL::Relocation<GetSelectedPower_t> value{
                REL::ID(116694)
            };
            return value;
        }

        auto& GetPowerEquipSlot()
        {
            static REL::Relocation<GetPowerEquipSlot_t> value{
                REL::ID(44544)
            };
            return value;
        }

        auto& UseActionSpell()
        {
            static REL::Relocation<UseActionSpell_t> value{ REL::ID(101916) };
            return value;
        }

        auto& PostUseSpell()
        {
            static REL::Relocation<PostUseSpell_t> value{ REL::ID(113904) };
            return value;
        }

        auto& QuantumEssence()
        {
            static REL::Relocation<RE::TESBoundObject**> value{
                REL::ID(1141364)
            };
            return value;
        }

        bool selectedReady = false;
        bool fmeReady = false;
    }

    bool ValidateSelected()
    {
        if (selectedReady) {
            return true;
        }

        auto& getSelected = GetSelectedPower();
        selectedReady =
            getSelected.offset() == 0x1FC6FB0 &&
            REL::Pattern<
                "48 83 EC 28 41 83 F9 04 73 30 41 0F B6 C1">()
                .match(getSelected.address());
        if (!selectedReady) {
            REX::ERROR("Selected-Power helper is unsupported");
        }
        return selectedReady;
    }

    bool ValidateFme()
    {
        if (fmeReady) {
            return true;
        }

        auto& unequip = UnequipSpell();
        auto& equipSlot = GetPowerEquipSlot();
        auto& action = UseActionSpell();
        auto& postUse = PostUseSpell();
        auto& essence = QuantumEssence();
        fmeReady =
            ValidateSelected() &&
            unequip.offset() == 0x19A2AA0 &&
            equipSlot.offset() == 0x4FC110 &&
            action.offset() == 0x199D9E0 &&
            postUse.offset() == 0x1EBC4F0 &&
            essence.offset() == 0x5EFA2E0 &&
            REL::Pattern<
                "48 85 D2 0F 84 A1 01 00 00 48 89 5C 24 08">()
                .match(unequip.address()) &&
            REL::Pattern<
                "8B 05 AE 4F 98 05 3B 05 C4 46 41 05">()
                .match(equipSlot.address()) &&
            REL::Pattern<
                "48 85 D2 0F 84 93 00 00 00 48 89 5C 24 08">()
                .match(action.address()) &&
            REL::Pattern<
                "48 89 5C 24 18 55 56 57 48 8B EC 48 83 EC 40">()
                .match(postUse.address());
        if (!fmeReady) {
            REX::ERROR("FME Power helpers are unsupported");
        }
        return fmeReady;
    }

    bool IsSelected(RE::SpellItem* a_spell, RE::Actor* a_actor)
    {
        return selectedReady &&
            a_spell &&
            a_actor &&
            GetSelectedPower()(nullptr, 0, a_actor, kPowerSlot) == a_spell;
    }

    bool TryHandle(
        RE::TESBoundObject* a_form,
        RE::PlayerCharacter* a_player,
        RE::ActorEquipManager* a_equipManager)
    {
        auto* spell = a_form ? a_form->As<RE::SpellItem>() : nullptr;
        if (!fmeReady || !spell || !a_player || !a_equipManager) {
            return false;
        }

        if (auto* essence = *QuantumEssence();
            essence && spell == essence) {
            UseActionSpell()(a_player, spell);
            PostUseSpell()(spell);
            return true;
        }

        if (!Settings::Powers() || !IsSelected(spell, a_player)) {
            return false;
        }

        const auto* slot = GetPowerEquipSlot()(
            static_cast<std::uint8_t>(kPowerSlot));
        if (!slot) {
            return false;
        }

        UnequipSpell()(a_equipManager, a_player, spell, slot);
        PostUseSpell()(spell);
        return true;
    }
}

// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <RE/Starfield.h>
#include <SFSE/SFSE.h>

#include <REL/ASM.h>
#include <REL/Relocation.h>
#include <REL/Utility.h>

#include <cstdint>
#include <limits>

namespace FavoritesToggle
{
    [[nodiscard]] bool InstallVanilla();
    [[nodiscard]] bool InstallFme();
    [[nodiscard]] bool RegisterMenuBridge(
        const SFSE::MenuInterface* a_menuInterface);

    namespace Settings
    {
        void Load();
        [[nodiscard]] bool Allows(const RE::TESForm* a_form);
        [[nodiscard]] bool Powers();
    }

    namespace Power
    {
        [[nodiscard]] bool ValidateSelected();
        [[nodiscard]] bool ValidateFme();
        [[nodiscard]] bool IsSelected(
            RE::SpellItem* a_spell,
            RE::Actor* a_actor);
        [[nodiscard]] bool TryHandle(
            RE::TESBoundObject* a_form,
            RE::PlayerCharacter* a_player,
            RE::ActorEquipManager* a_equipManager);
    }

    namespace detail
    {
        [[nodiscard]] inline bool FitsCall5(
            const std::uintptr_t a_callsite,
            const std::uintptr_t a_target) noexcept
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
    }
}

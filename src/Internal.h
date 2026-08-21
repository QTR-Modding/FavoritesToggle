// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <RE/Starfield.h>
#include <SFSE/SFSE.h>

namespace FavoritesToggle
{
    [[nodiscard]] bool InstallVanilla();
    [[nodiscard]] bool InstallFavoritesMenuExtended(
        const SFSE::MenuInterface* a_menuInterface);

    namespace Toggle
    {
        [[nodiscard]] bool TryUnequip(
            RE::TESBoundObject* a_form,
            RE::PlayerCharacter* a_player);
    }

    namespace Power
    {
        [[nodiscard]] bool IsSelected(
            RE::SpellItem* a_spell,
            RE::Actor* a_actor);
    }
}

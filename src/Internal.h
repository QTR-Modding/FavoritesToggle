// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <RE/Starfield.h>
#include <SFSE/SFSE.h>

namespace FavoritesToggle
{
    [[nodiscard]] bool InstallVanilla();

    namespace Settings
    {
        void Load();
        [[nodiscard]] bool Allows(const RE::TESForm* a_form);
        [[nodiscard]] bool Powers();
    }

    namespace Power
    {
        [[nodiscard]] bool ValidateSelected();
        [[nodiscard]] bool IsSelected(
            RE::SpellItem* a_spell,
            RE::Actor* a_actor);
    }
}

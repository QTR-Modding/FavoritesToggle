// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

namespace FavoritesToggle::Power
{
    namespace
    {
        constexpr auto kPowerSource =
            RE::MagicSystem::CastingSource::kOther;
    }

    bool IsSelected(RE::SpellItem* a_spell, RE::Actor* a_actor)
    {
        return a_spell &&
            a_actor &&
            a_actor->GetEquippedSpell(kPowerSource) == a_spell;
    }

}

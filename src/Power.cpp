// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

#include <REL/Pattern.h>

namespace FavoritesToggle::Power
{
    namespace
    {
        constexpr std::uint32_t kPowerSlot = 2;

        using GetSelectedPower_t = RE::SpellItem* (*)(
            void*,
            std::uint32_t,
            RE::Actor*,
            std::uint32_t);

        auto& GetSelectedPower()
        {
            static REL::Relocation<GetSelectedPower_t> value{
                REL::ID(116694)
            };
            return value;
        }

        bool selectedReady = false;
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

    bool IsSelected(RE::SpellItem* a_spell, RE::Actor* a_actor)
    {
        return selectedReady &&
            a_spell &&
            a_actor &&
            GetSelectedPower()(nullptr, 0, a_actor, kPowerSlot) == a_spell;
    }

}

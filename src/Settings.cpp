// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

#include <REX/FIniSettingStore.h>
#include <REX/TIniSetting.h>

namespace FavoritesToggle::Settings
{
    namespace
    {
        REX::TIniSetting<bool> weapons{ "Toggle", "Weapons", true };
        REX::TIniSetting<bool> armor{ "Toggle", "Armor", true };
        REX::TIniSetting<bool> powers{ "Toggle", "Powers", true };
        REX::TIniSetting<bool> other{
            "Toggle",
            "OtherEquippedItems",
            true
        };
    }

    void Load()
    {
        auto* store = REX::FIniSettingStore::GetSingleton();
        store->Init(
            "Data/SFSE/Plugins/FavoritesToggle.ini",
            "Data/SFSE/Plugins/FavoritesToggleCustom.ini");
        store->Load();
    }

    bool Allows(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return false;
        }

        switch (a_form->GetFormType()) {
        case RE::FormType::kWEAP:
            return weapons.GetValue();
        case RE::FormType::kARMO:
            return armor.GetValue();
        default:
            return other.GetValue();
        }
    }

    bool Powers()
    {
        return powers.GetValue();
    }
}

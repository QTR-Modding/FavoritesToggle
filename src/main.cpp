// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

namespace FavoritesToggle
{
    void OnPostLoad(SFSE::MessagingInterface::Message* a_message)
    {
        if (a_message->type != SFSE::MessagingInterface::kPostLoad) {
            return;
        }

        const bool vanilla = InstallVanilla();
        const bool fme = InstallFme();
        if (!vanilla || !fme) {
            REX::ERROR(
                "Favorites Toggle loaded with inactive features: "
                "Vanilla={}, FME={}",
                vanilla,
                fme);
        }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    SFSE::InitInfo initInfo{
        .logPattern = "%Y-%m-%d %H:%M:%S.%e [%l] %v",
        .trampoline = true,
        .trampolineSize = 2 * sizeof(REL::ASM::JMP14)
    };
    SFSE::Init(a_sfse, initInfo);
    FavoritesToggle::Settings::Load();

    const auto* messaging = SFSE::GetMessagingInterface();
    const auto* menu = SFSE::GetMenuInterface();
    if (!messaging || !menu) {
        REX::ERROR("Required SFSE interface is unavailable");
        return false;
    }

    if (!messaging->RegisterListener(FavoritesToggle::OnPostLoad) ||
        !FavoritesToggle::RegisterMenuBridge(menu)) {
        return false;
    }

    REX::INFO(
        "Favorites Toggle {} loaded for Starfield {}",
        SFSE::GetPluginVersion().string("."),
        a_sfse->RuntimeVersion().string("."));
    return true;
}

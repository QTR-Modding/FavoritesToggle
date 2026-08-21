// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

#include <REL/ASM.h>

namespace FavoritesToggle
{
    void OnPostLoad(SFSE::MessagingInterface::Message* a_message)
    {
        if (a_message->type != SFSE::MessagingInterface::kPostLoad) {
            return;
        }

        if (!InstallVanilla()) {
            REX::ERROR("Favorites Toggle is inactive");
        }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    SFSE::InitInfo initInfo{
        .logPattern = "%Y-%m-%d %H:%M:%S.%e [%l] %v",
        .trampoline = true,
        .trampolineSize = sizeof(REL::ASM::JMP14)
    };
    SFSE::Init(a_sfse, initInfo);

    const auto* messaging = SFSE::GetMessagingInterface();
    if (!messaging) {
        REX::ERROR("SFSE messaging interface is unavailable");
        return false;
    }

    if (!messaging->RegisterListener(FavoritesToggle::OnPostLoad)) {
        return false;
    }

    REX::INFO(
        "Favorites Toggle {} loaded for Starfield {}",
        SFSE::GetPluginVersion().string("."),
        a_sfse->RuntimeVersion().string("."));
    return true;
}

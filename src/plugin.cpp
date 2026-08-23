// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PCH.h"
#include "Hooks.h"

namespace FavoritesToggle
{
    void OnPostLoad(SFSE::MessagingInterface::Message* a_message) noexcept
    {
        if (!a_message || a_message->type != SFSE::MessagingInterface::kPostLoad) {
            return;
        }

        try {
            if (!Vanilla::Install()) {
                logger::error("Favorites Toggle is inactive");
                return;
            }
        } catch (...) {
            logger::error("Favorites Toggle installation failed");
            return;
        }

        try {
            if (!FME::Install()) {
                logger::warn("Favorites Menu Extended compatibility is inactive");
            }
        } catch (...) {
            logger::warn("Favorites Menu Extended compatibility failed to install");
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
    if (!messaging) {
        logger::error("SFSE messaging interface is unavailable");
        return false;
    }
    if (!messaging->RegisterListener(FavoritesToggle::OnPostLoad)) {
        logger::error("Could not register the SFSE message listener");
        return false;
    }

    logger::info("Loaded for Starfield {}", a_sfse->RuntimeVersion().string("."));
    return true;
}

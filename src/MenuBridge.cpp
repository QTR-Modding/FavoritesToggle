// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

#include <string_view>

namespace FavoritesToggle
{
    namespace
    {
        using Value = RE::Scaleform::GFx::Value;

        class FavoritesUseListener final :
            public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                auto* root = a_params.movie &&
                        a_params.movie->asMovieRoot ?
                    a_params.movie->asMovieRoot.get() :
                    nullptr;
                if (!root) {
                    return;
                }

                Value menu;
                Value selectedIndex;
                if (root->GetVariable(&menu, "root1.Menu_mc") &&
                    menu.IsObject() &&
                    menu.GetMember("selectedIndex", &selectedIndex)) {
                    menu.SetMember(
                        "iLastSelectedQuickKey",
                        selectedIndex);
                }
            }
        };

        // Scaleform owns references to the function; retain its native handler
        // for the process lifetime.
        auto* listener = new FavoritesUseListener{};

        void OnMenuMovieCreated(RE::IMenu* a_menu)
        {
            const auto* name = a_menu ? a_menu->GetName() : nullptr;
            if (!name ||
                std::string_view{ name } != "FavoritesMenu" ||
                !a_menu->uiMovie ||
                !a_menu->uiMovie->asMovieRoot) {
                return;
            }

            auto* root = a_menu->uiMovie->asMovieRoot.get();
            Value menu;
            Value loaderInfo;
            Value domain;
            Value manager;
            if (!root->GetVariable(&menu, a_menu->GetRootPath()) ||
                !menu.IsObject() ||
                !menu.GetMember("loaderInfo", &loaderInfo) ||
                !loaderInfo.IsObject() ||
                !loaderInfo.GetMember("applicationDomain", &domain) ||
                !domain.IsObject()) {
                REX::WARN("FavoritesMenu bridge could not resolve its domain");
                return;
            }

            Value className;
            root->CreateString(
                &className,
                "Shared.AS3.Data::BSUIDataManager");
            if (!domain.Invoke(
                    "getDefinition",
                    &manager,
                    &className,
                    1) ||
                !manager.IsObject()) {
                REX::WARN("FavoritesMenu bridge could not resolve its manager");
                return;
            }

            Value callback;
            Value eventName;
            root->CreateFunction(&callback, listener);
            root->CreateString(
                &eventName,
                "FavoritesMenu_UseQuickkey");
            Value args[2]{ eventName, callback };
            if (!manager.Invoke("addEventListener", nullptr, args, 2)) {
                REX::WARN("FavoritesMenu bridge could not attach its listener");
            }
        }
    }

    bool RegisterMenuBridge(const SFSE::MenuInterface* a_menuInterface)
    {
        if (!a_menuInterface) {
            return false;
        }

        a_menuInterface->Register(OnMenuMovieCreated);
        REX::INFO("Registered FavoritesMenu input bridge");
        return true;
    }
}

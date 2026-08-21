// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

namespace FavoritesToggle::Toggle
{
    bool TryUnequip(
        RE::TESBoundObject* a_form,
        RE::PlayerCharacter* a_player)
    {
        if (!a_form || !a_player) {
            return false;
        }

        RE::TESBoundObject* equippedObject = nullptr;
        RE::BSTSmartPointer<RE::TBO_InstanceData> instanceData;
        bool locked = false;
        a_player->ForEachEquippedItem(
            [&](const RE::BGSInventoryItem& a_item) {
                if (a_item.object != a_form) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                equippedObject = a_item.object;
                instanceData = a_item.instanceData;
                locked = a_item.flags.any(
                    RE::BGSInventoryItem::Flag::kEquipStateLocked);
                return RE::BSContainer::ForEachResult::kStop;
            });

        if (!equippedObject) {
            return false;
        }
        if (locked) {
            return true;
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) {
            return false;
        }

        const RE::BGSObjectInstance instance(
            equippedObject,
            instanceData.get());
        equipManager->UnequipObject(
            a_player,
            instance,
            nullptr,
            true,
            false,
            true,
            false,
            nullptr);
        return true;
    }
}

// Copyright (C) 2026 Quantumyilmaz
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Internal.h"

#include <REL/Pattern.h>
#include <REX/FModule.h>

#include <array>
#include <cstring>

namespace FavoritesToggle
{
    namespace
    {
        constexpr std::string_view kModuleName =
            "FavoritesMenuExtended.dll";
        constexpr std::uintptr_t kUseFavoriteRva = 0x25730;
        constexpr std::array<std::uintptr_t, 4> kCallRvas{
            0x28871,
            0x28B3D,
            0x2B7A9,
            0x2BA7D
        };

        using UseFavorite_t = void (*)(RE::TESBoundObject*);
        REL::Relocation<UseFavorite_t> originalUseFavorite;

        bool Contains(
            const REX::FModuleSection& a_section,
            const std::uintptr_t a_address,
            const std::size_t a_size) noexcept
        {
            const auto begin = a_section.GetAddress();
            if (!begin || a_address < begin) {
                return false;
            }

            const auto offset = a_address - begin;
            return offset <= a_section.GetSize() &&
                a_size <= a_section.GetSize() - offset;
        }

        bool TryUnequip(RE::TESBoundObject* a_form)
        {
            if (!a_form) {
                return false;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!player || !equipManager) {
                return false;
            }

            if (Power::TryHandle(a_form, player, equipManager)) {
                return true;
            }
            if (a_form->Is<RE::SpellItem>()) {
                return false;
            }

            RE::TESBoundObject* equippedObject = nullptr;
            RE::BSTSmartPointer<RE::TBO_InstanceData> instanceData;
            bool locked = false;
            player->ForEachEquippedItem(
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
            if (!Settings::Allows(equippedObject)) {
                return false;
            }
            if (locked) {
                return true;
            }

            const RE::BGSObjectInstance instance(
                equippedObject,
                instanceData.get());
            equipManager->UnequipObject(
                player,
                instance,
                nullptr,
                true,
                false,
                true,
                false,
                nullptr);
            return true;
        }

        void UseFavorite(RE::TESBoundObject* a_form) noexcept
        {
            try {
                if (TryUnequip(a_form)) {
                    return;
                }
            } catch (...) {
                REX::ERROR(
                    "Favorite toggle failed; preserving FME behavior");
            }

            originalUseFavorite(a_form);
        }
    }

    bool InstallFme()
    {
        const auto module = REX::FModule::GetLoadedModule(kModuleName);
        const auto base = module.GetBaseAddress();
        if (!base) {
            REX::INFO("Favorites Menu Extended is not loaded");
            return true;
        }

        const auto version = REL::GetFileVersion(module.GetFileName());
        if (!version ||
            *version != REL::Version{ 5, 0, 0, 0 } ||
            !Power::ValidateFme()) {
            REX::ERROR("Favorites Menu Extended 5.0 is required");
            return false;
        }

        const auto text = module.GetSection(".text");
        const auto originalTarget = base + kUseFavoriteRva;
        if (!Contains(text, originalTarget, 22) ||
            !REL::Pattern<
                "48 89 5C 24 10 48 89 6C 24 18 56 57 "
                "41 54 41 56 41 57 48 83 EC 70">()
                .match(originalTarget)) {
            REX::ERROR("FME use-favorite signature mismatch");
            return false;
        }

        for (const auto rva : kCallRvas) {
            const auto callsite = base + rva;
            if (!Contains(text, callsite, sizeof(REL::ASM::CALL5)) ||
                *reinterpret_cast<const std::uint8_t*>(callsite) != 0xE8 ||
                REL::ASM::CALL5::TARGET(callsite) != originalTarget) {
                REX::ERROR(
                    "FME callsite mismatch at RVA 0x{:X}",
                    rva);
                return false;
            }
        }

        originalUseFavorite = originalTarget;
        const auto wrapper =
            reinterpret_cast<std::uintptr_t>(&UseFavorite);
        for (const auto rva : kCallRvas) {
            if (!detail::FitsCall5(base + rva, wrapper)) {
                REX::ERROR(
                    "FME hook is outside rel32 range at RVA 0x{:X}",
                    rva);
                return false;
            }
        }

        constexpr auto size = sizeof(REL::ASM::CALL5);
        std::array<
            std::array<std::uint8_t, size>,
            kCallRvas.size()> originals{};
        for (std::size_t i = 0; i < kCallRvas.size(); ++i) {
            std::memcpy(
                originals[i].data(),
                reinterpret_cast<const void*>(base + kCallRvas[i]),
                size);
        }

        const auto rollback = [&](const std::size_t a_count) {
            bool restored = true;
            for (std::size_t i = a_count; i-- > 0;) {
                const auto callsite = base + kCallRvas[i];
                restored =
                    REL::WriteSafeData(callsite, originals[i]) &&
                    std::memcmp(
                        reinterpret_cast<const void*>(callsite),
                        originals[i].data(),
                        size) == 0 &&
                    restored;
            }
            return restored;
        };

        for (std::size_t i = 0; i < kCallRvas.size(); ++i) {
            const auto callsite = base + kCallRvas[i];
            const REL::ASM::CALL5 replacement{ callsite, wrapper };
            if (!REL::WriteSafeData(callsite, replacement) ||
                std::memcmp(
                    reinterpret_cast<const void*>(callsite),
                    &replacement,
                    size) != 0) {
                const bool restored = rollback(i + 1);
                REX::ERROR(
                    "FME hook failed at RVA 0x{:X}; rollback={}",
                    kCallRvas[i],
                    restored);
                return false;
            }
        }

        REX::INFO("Enabled Favorites Menu Extended 5.0 toggling");
        return true;
    }
}

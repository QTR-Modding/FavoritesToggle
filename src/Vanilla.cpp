#include "Hooks.h"

#include <cstring>

namespace FavoritesToggle::Vanilla
{
	namespace
	{
		constexpr std::ptrdiff_t kInventoryCallOffset = 0x2F6;
		constexpr std::ptrdiff_t kSpellStateCallOffset = 0x377;

		using UseInventoryItem = bool (*)(RE::ActorEquipManager*, RE::Actor*, const RE::InventoryInterface::Handle*,
            std::uint8_t, bool, bool, bool);
		using SpellState = bool (*)(void*, RE::Actor*, RE::SpellItem*);

		UseInventoryItem originalUseInventoryItem{};
		SpellState originalSpellState{};

		bool UseInventoryItemHook(
			RE::ActorEquipManager*                a_equipManager,
			RE::Actor*                            a_actor,
			const RE::InventoryInterface::Handle* a_handle,
			std::uint8_t                          a_unk3,
			bool                                  a_allowUnequip,
			bool                                  a_unk5,
			bool                                  a_unk6) noexcept
		{
			const auto inventory = RE::BGSInventoryInterface::GetSingleton();
			if (inventory && a_handle) {
				if (const auto form = inventory->GetInventoryObject(a_handle->id)) {
					a_allowUnequip = Settings::Allows(*form);
				}
			}

			return originalUseInventoryItem(
				a_equipManager,
				a_actor,
				a_handle,
				a_unk3,
				a_allowUnequip,
				a_unk5,
				a_unk6);
		}

		bool FilterSpellState(
			void*          a_context,
			RE::Actor*     a_actor,
			RE::SpellItem* a_spell) noexcept
		{
			if (Settings::Powers() && a_spell && a_actor &&
				a_actor->GetEquippedSpell(RE::MagicSystem::CastingSource::kOther) == a_spell) {
				return false;
			}

			return originalSpellState(a_context, a_actor, a_spell);
		}
	}

	bool Install()
	{
		constexpr std::uint8_t inventoryBefore[]{
			0xC6, 0x44, 0x24, 0x30, 0x01, 0xC6, 0x44, 0x24, 0x28, 0x00, 0xC6, 0x44, 0x24, 0x20, 0x00,
			0x4C, 0x8D, 0x45, 0x67, 0x48, 0x8B, 0x15, 0xC1, 0x3B, 0x08, 0x04, 0x48, 0x8B, 0x0D, 0xCA,
			0x2E, 0x32, 0x04
		};
		constexpr std::uint8_t inventoryAfter[]{ 0x0F, 0xB6, 0xF8 };
		constexpr std::uint8_t spellBefore[]{ 0x4C, 0x8B, 0xC3, 0x48, 0x8B, 0x15, 0x39, 0x3B, 0x08, 0x04 };
		constexpr std::uint8_t spellAfter[]{ 0x84, 0xC0, 0x0F, 0x85, 0xF8, 0x00, 0x00, 0x00 };
		constexpr std::uint8_t spellPrologue[]{
			0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18,
			0x48, 0x89, 0x74, 0x24, 0x20, 0x48, 0x89, 0x4C, 0x24, 0x08
		};
		const auto useInventoryItem = SF1::ResolveID(101954);
		const auto useQuickkey = SF1::ResolveID(RE::FavoritesManager::kUseQuickkeyID.value);
		const auto inventoryCall = useQuickkey + kInventoryCallOffset;
		const auto spellCall = useQuickkey + kSpellStateCallOffset;
		const auto spellState = SF1::Hooks::CallTarget(spellCall);
		const auto matches = [](std::uintptr_t address, std::span<const std::uint8_t> bytes) {
			return std::memcmp(reinterpret_cast<const void*>(address), bytes.data(), bytes.size()) == 0;
		};
		if (!matches(inventoryCall - sizeof(inventoryBefore), inventoryBefore) ||
			!matches(inventoryCall + SF1::Hooks::callSize, inventoryAfter) ||
			SF1::Hooks::CallTarget(inventoryCall) != useInventoryItem) {
			logger::error("Vanilla inventory hook signature mismatch");
			return false;
		}
		if (!spellState || !matches(spellCall - sizeof(spellBefore), spellBefore) ||
			!matches(spellCall + SF1::Hooks::callSize, spellAfter) ||
			!matches(spellState, spellPrologue)) {
			logger::error("Vanilla Power hook signature mismatch");
			return false;
		}

		auto& trampoline = REL::GetTrampoline();
		const auto inventoryRelay = SF1::Hooks::AllocateCallRelay(trampoline,
			reinterpret_cast<std::uintptr_t>(&UseInventoryItemHook));
		const auto spellRelay = SF1::Hooks::AllocateCallRelay(trampoline,
			reinterpret_cast<std::uintptr_t>(&FilterSpellState));
		if (!SF1::Hooks::CanReachCall(inventoryCall, inventoryRelay) ||
			!SF1::Hooks::CanReachCall(spellCall, spellRelay)) {
			logger::error("Vanilla favorite hook relay is unavailable");
			return false;
		}

		originalUseInventoryItem = reinterpret_cast<UseInventoryItem>(useInventoryItem);
		originalSpellState = reinterpret_cast<SpellState>(spellState);
		if (!SF1::Hooks::WriteCall(inventoryCall, inventoryRelay) || !SF1::Hooks::WriteCall(spellCall, spellRelay)) {
			const auto restore = [](std::uintptr_t site, std::uintptr_t relay, std::uintptr_t original) {
				const auto current = SF1::Hooks::CallTarget(site);
				return current == original || (current == relay && SF1::Hooks::WriteCall(site, original));
			};
			const bool inventoryRestored = restore(inventoryCall, inventoryRelay, useInventoryItem);
			const bool spellRestored = restore(spellCall, spellRelay, spellState);
			logger::error("Vanilla hook installation failed; rollback verified: {}", inventoryRestored && spellRestored);
			return false;
		}

		logger::info("Installed vanilla favorite toggles");
		return true;
	}
}

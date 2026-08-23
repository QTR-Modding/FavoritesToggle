#include "Hooks.h"

namespace FavoritesToggle::Vanilla
{
	namespace
	{
		constexpr std::ptrdiff_t kInventoryCallOffset = 0x2F6;
		constexpr std::ptrdiff_t kSpellStateCallOffset = 0x377;

		using UseInventoryItem = decltype(&RE::ActorEquipManager::UseInventoryItem);
		using SpellState = bool (*)(void*, RE::Actor*, RE::SpellItem*);

		REL::Relocation<UseInventoryItem> originalUseInventoryItem;
		REL::Relocation<SpellState>       originalSpellState;

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
		const REL::Relocation<UseInventoryItem> useInventoryItem{
			RE::ID::ActorEquipManager::UseInventoryItem
		};
		REL::Relocation inventoryCall{
			RE::ID::FavoritesManager::UseQuickkey,
			kInventoryCallOffset
		};
		REL::Relocation spellCall{
			RE::ID::FavoritesManager::UseQuickkey,
			kSpellStateCallOffset
		};
		const auto spellState = REL::ASM::CALL5::TARGET(spellCall.address());

		if (!REL::Pattern<
				"C6 44 24 30 01 C6 44 24 28 00 C6 44 24 20 00 "
				"4C 8D 45 67 48 8B 15 C1 3B 08 04 "
				"48 8B 0D CA 2E 32 04 E8 ?? ?? ?? ?? 0F B6 F8">()
				.match(inventoryCall.address() - 33) ||
			REL::ASM::CALL5::TARGET(inventoryCall.address()) != useInventoryItem.address()) {
			logger::error("Vanilla inventory hook signature mismatch");
			return false;
		}
		if (!REL::Pattern<
				"4C 8B C3 48 8B 15 39 3B 08 04 "
				"E8 ?? ?? ?? ?? 84 C0 0F 85 F8 00 00 00">()
				.match(spellCall.address() - 10) ||
			!REL::Pattern<
				"48 89 5C 24 10 48 89 6C 24 18 "
				"48 89 74 24 20 48 89 4C 24 08">()
				.match(spellState)) {
			logger::error("Vanilla Power hook signature mismatch");
			return false;
		}

		auto& trampoline = REL::GetTrampoline();
		const auto inventoryRelay = trampoline.allocate_branch5(
			reinterpret_cast<std::uintptr_t>(&UseInventoryItemHook));
		const auto spellRelay = trampoline.allocate_branch5(
			reinterpret_cast<std::uintptr_t>(&FilterSpellState));

		originalUseInventoryItem = inventoryCall.write_call<5>(&UseInventoryItemHook);
		originalSpellState = spellCall.write_call<5>(&FilterSpellState);
		if (originalUseInventoryItem.address() != useInventoryItem.address() ||
			originalSpellState.address() != spellState ||
			REL::ASM::CALL5::TARGET(inventoryCall.address()) != inventoryRelay ||
			REL::ASM::CALL5::TARGET(spellCall.address()) != spellRelay) {
			logger::error("Vanilla hook installation failed");
			return false;
		}

		logger::info("Installed vanilla favorite toggles");
		return true;
	}
}

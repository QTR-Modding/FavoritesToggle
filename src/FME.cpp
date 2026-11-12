#include "Hooks.h"

#include <cstring>

namespace FavoritesToggle::FME
{
	namespace
	{
		using Value = RE::Scaleform::GFx::Value;
		using UseFavorite = void (*)(RE::TESBoundObject*);

		constexpr auto                          kModuleName = L"FavoritesMenuExtended.dll";
		constexpr std::uintptr_t                kUseFavoriteRva = 0x25730;
		constexpr std::array<std::uintptr_t, 4> kCallRvas{ 0x28871, 0x28B3D, 0x2B7A9, 0x2BA7D };
		constexpr auto                          kPowerSource = RE::MagicSystem::CastingSource::kOther;
		constexpr std::uint32_t                 kConfirmKeyCode = 13;

		UseFavorite originalUseFavorite{};
		std::atomic_bool             enabled{ false };
		std::atomic_uint64_t         activationGeneration{ 0 };
		std::atomic_uint64_t         consumedGeneration{ 0 };

		[[nodiscard]] bool Contains(
			const REX::FModuleSection& a_section,
			const std::uintptr_t a_address,
			const std::size_t a_size) noexcept
		{
			const auto begin = a_section.GetAddress();
			if (!begin || a_address < begin) {
				return false;
			}

			const auto offset = a_address - begin;
			return offset <= a_section.GetSize() && a_size <= a_section.GetSize() - offset;
		}

		[[nodiscard]] RE::SpellItem* GetQuantumEssence()
		{
			static RE::SpellItem* spell = nullptr;
			if (!spell) {
				const auto form = RE::TESForm::LookupByEditorID(RE::BSFixedString{ "QuantumEssenceSpell_DO" });
				if (const auto defaultObject = form ? form->As<RE::BGSDefaultObject>() : nullptr) {
					if (const auto essence = defaultObject->Object()) {
						spell = essence->As<RE::SpellItem>();
					}
				}
			}
			return spell;
		}

		[[nodiscard]] bool TryUnequip(RE::TESBoundObject& a_form, RE::PlayerCharacter& a_player)
		{
			RE::BSTSmartPointer<RE::TBO_InstanceData> instanceData;
			bool                                      found = false;
			bool                                      locked = false;
			a_player.ForEachEquippedItem([&](const RE::BGSInventoryItem& a_item) {
				if (a_item.object != &a_form) {
					return RE::BSContainer::ForEachResult::kContinue;
				}

				found = true;
				instanceData = a_item.InstanceData();
				locked = a_item.IsEquipStateLocked();
				return RE::BSContainer::ForEachResult::kStop;
			});

			if (!found) {
				return false;
			}
			if (locked) {
				return true;
			}

			const auto equipManager = RE::ActorEquipManager::GetSingleton();
			if (!equipManager) {
				return false;
			}

			const RE::BGSObjectInstance instance(&a_form, instanceData.get());
			if (!equipManager->UnequipObject(&a_player, instance, nullptr, true, false, true, false, nullptr)) {
				logger::debug("Could not unequip the selected favorite");
			}
			return true;
		}

		[[nodiscard]] bool TryHandlePower(RE::SpellItem& a_spell, RE::PlayerCharacter& a_player)
		{
			if (&a_spell == GetQuantumEssence()) {
				a_player.UseSpell(&a_spell);
				(void)a_spell.PlayPowerSelectionSound();
				return true;
			}
			if (a_player.GetEquippedSpell(kPowerSource) != &a_spell) {
				return false;
			}

			a_player.UnequipSpell(&a_spell, kPowerSource);
			(void)a_spell.PlayPowerSelectionSound();
			return true;
		}

		void BeginMenuActivation() noexcept
		{
			activationGeneration.fetch_add(1, std::memory_order_release);
		}

		[[nodiscard]] bool IsRepeatedMenuActivation() noexcept
		{
			const auto  generation = activationGeneration.load(std::memory_order_acquire);
			const auto ui = RE::UI::GetSingleton();
			if (!generation || !ui || !ui->IsMenuOpen(RE::BSFixedString{ "FavoritesMenu" })) {
				return false;
			}

			return consumedGeneration.exchange(generation, std::memory_order_acq_rel) == generation;
		}

		void UseFavoriteHook(RE::TESBoundObject* a_form) noexcept
		{
			if (enabled.load(std::memory_order_acquire) && a_form) {
				if (IsRepeatedMenuActivation()) {
					return;
				}
				if (const auto player = RE::PlayerCharacter::GetSingleton()) {
					if (const auto spell = a_form->As<RE::SpellItem>()) {
						if (Settings::Powers() && TryHandlePower(*spell, *player)) {
							return;
						}
					} else if (Settings::Allows(*a_form) && TryUnequip(*a_form, *player)) {
						return;
					}
				}
			}
			originalUseFavorite(a_form);
		}

		class FavoritesUseListener final : public RE::Scaleform::GFx::FunctionHandler
		{
		public:
			explicit FavoritesUseListener(const bool a_confirmOnly) :
				confirmOnly_(a_confirmOnly)
			{}

			void Call(const Params& a_params) override
			{
				if (confirmOnly_ && !IsConfirmEvent(a_params)) {
					return;
				}

				const auto root = a_params.movie ? a_params.movie->asMovieRoot.get() : nullptr;
				if (!root) {
					return;
				}

				Value menu;
				Value selectedIndex;
				if (root->GetVariable(&menu, "root1.Menu_mc") && menu.IsObject() &&
					menu.GetMember("selectedIndex", &selectedIndex)) {
					menu.SetMember("iLastSelectedQuickKey", selectedIndex);
					BeginMenuActivation();
				}
			}

		private:
			[[nodiscard]] static bool IsConfirmEvent(const Params& a_params)
			{
				if (!a_params.args || !a_params.argCount || !a_params.args[0].IsObject()) {
					return false;
				}

				Value keyCode;
				if (!a_params.args[0].GetMember("keyCode", &keyCode)) {
					return false;
				}
				if (keyCode.IsUInt()) {
					return keyCode.GetUInt() == kConfirmKeyCode;
				}
				if (keyCode.IsInt()) {
					return std::cmp_equal(keyCode.GetInt(), kConfirmKeyCode);
				}
				return keyCode.IsNumber() && keyCode.GetNumber() == kConfirmKeyCode;
			}

			bool confirmOnly_;
		};

		void OnMenuMovieCreated(RE::IMenu* a_menu) noexcept
		{
			try {
				const auto name = a_menu ? a_menu->GetName() : nullptr;
				if (!name || std::string_view{ name } != "FavoritesMenu" || !a_menu->uiMovie ||
					!a_menu->uiMovie->asMovieRoot) {
					return;
				}

				auto root = a_menu->uiMovie->asMovieRoot.get();
				activationGeneration.store(0, std::memory_order_release);
				consumedGeneration.store(0, std::memory_order_release);
				Value menu;
				if (!root->GetVariable(&menu, "root1.Menu_mc") || !menu.IsObject()) {
					return;
				}

				Value             mouseCallback;
				Value             keyCallback;
				static const auto mouseListener =
					RE::Scaleform::make_shared<FavoritesUseListener>(false);
				static const auto keyListener =
					RE::Scaleform::make_shared<FavoritesUseListener>(true);
				root->CreateFunction(&mouseCallback, mouseListener.get(), nullptr);
				root->CreateFunction(&keyCallback, keyListener.get(), nullptr);
				Value mouseEvent;
				Value keyEvent;
				root->CreateString(&mouseEvent, "FavoritesEntry::mouse_click");
				root->CreateString(&keyEvent, "keyUp");
				const Value useCapture{ false };
				const Value priority{ std::int32_t{ 100 } };
				Value       mouseArgs[4]{ mouseEvent, mouseCallback, useCapture, priority };
				Value       keyArgs[4]{ keyEvent, keyCallback, useCapture, priority };
				if (!menu.Invoke("addEventListener", nullptr, mouseArgs, 4) ||
					!menu.Invoke("addEventListener", nullptr, keyArgs, 4)) {
					logger::error("Favorites Menu Extended input bridge was not registered");
				}
			} catch (const std::exception& error) {
				logger::error("Favorites Menu Extended menu setup failed: {}", error.what());
			}
		}
	}

	bool Install()
	{
		const auto module = REX::FModule::GetLoadedModule(kModuleName);
		const auto base = module.GetBaseAddress();
		if (!base) {
			return true;
		}

		if (module.GetFileVersion() != (5U << 24)) {
			logger::error("Favorites Menu Extended compatibility requires version 5.0.0");
			return false;
		}
		const auto menuInterface = SFSE::GetMenuInterface();
		if (!menuInterface) {
			logger::error("SFSE menu interface is unavailable");
			return false;
		}
		const auto text = module.GetSection(".text");
		const auto originalTarget = base + kUseFavoriteRva;
		constexpr std::uint8_t prologue[]{
			0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x56,
			0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x70
		};
		if (!Contains(text, originalTarget, sizeof(prologue)) ||
			std::memcmp(reinterpret_cast<const void*>(originalTarget), prologue, sizeof(prologue)) != 0) {
			logger::error("Favorites Menu Extended use-favorite signature mismatch");
			return false;
		}

		for (const auto rva : kCallRvas) {
			const auto callsite = base + rva;
			if (!Contains(text, callsite, SF1::Hooks::callSize) ||
				SF1::Hooks::CallTarget(callsite) != originalTarget) {
				logger::error("Favorites Menu Extended callsite mismatch at RVA 0x{:X}", rva);
				return false;
			}
		}

		static SF1::Hooks::NearBranchPool trampoline;
		const auto anchor = base + kCallRvas[kCallRvas.size() / 2];
		if (!trampoline.Initialize(anchor, sizeof(REL::ASM::JMP14))) {
			logger::error("Favorites Menu Extended trampoline could not be allocated");
			return false;
		}
		const auto relay = SF1::Hooks::AllocateCallRelay(trampoline.GetTrampoline(),
			reinterpret_cast<std::uintptr_t>(&UseFavoriteHook));
		for (const auto rva : kCallRvas) {
			if (!SF1::Hooks::CanReachCall(base + rva, relay)) {
				logger::error("Favorites Menu Extended trampoline is unreachable");
				return false;
			}
		}

		originalUseFavorite = reinterpret_cast<UseFavorite>(originalTarget);
		for (const auto rva : kCallRvas) {
			if (!SF1::Hooks::WriteCall(base + rva, relay)) {
				bool restored = true;
				for (const auto restoreRva : kCallRvas) {
					const auto site = base + restoreRva;
					if (SF1::Hooks::CallTarget(site) == relay && !SF1::Hooks::WriteCall(site, originalTarget)) {
						restored = false;
					}
				}
				logger::error("Favorites Menu Extended hook failed at RVA 0x{:X}; rollback verified: {}", rva, restored);
				return false;
			}
		}

		menuInterface->Register(OnMenuMovieCreated);
		enabled.store(true, std::memory_order_release);
		logger::info("Enabled Favorites Menu Extended 5.0 compatibility");
		return true;
	}
}

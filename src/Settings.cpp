#include "Hooks.h"
#include "REX/REX.h"

namespace FavoritesToggle::Settings
{
	namespace
	{
		REX::TIniSetting weapons{ "Toggle", "Weapons", true };
		REX::TIniSetting armor{ "Toggle", "Armor", true };
		REX::TIniSetting powers{ "Toggle", "Powers", true };
	}

	void Load()
	{
		const auto store = REX::FIniSettingStore::GetSingleton();
		store->Init(
			"Data/SFSE/Plugins/FavoritesToggle.ini",
			"Data/SFSE/Plugins/FavoritesToggleCustom.ini");
		store->Load();
	}

	bool Allows(const RE::TESForm& a_form)
	{
		switch (a_form.GetFormType()) {
		case RE::FormType::kWEAP:
			return weapons.GetValue();
		case RE::FormType::kARMO:
			return armor.GetValue();
		default:
			return false;
		}
	}

	bool Powers()
	{
		return powers.GetValue();
	}
}

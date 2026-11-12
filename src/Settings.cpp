#include "Hooks.h"

#include <SimpleIni.h>

namespace FavoritesToggle::Settings
{
	namespace
	{
		bool weapons = true;
		bool armor = true;
		bool powers = true;
	}

	void Load()
	{
		CSimpleIniA config;
		config.SetUnicode(true);
		config.SetQuotes(true);
		const auto read = [&config] {
			weapons = config.GetBoolValue("Toggle", "Weapons", weapons);
			armor = config.GetBoolValue("Toggle", "Armor", armor);
			powers = config.GetBoolValue("Toggle", "Powers", powers);
		};
		if (config.LoadFile("Data/SFSE/Plugins/FavoritesToggle.ini") >= 0) {
			read();
		}
		if (config.LoadFile("Data/SFSE/Plugins/FavoritesToggleCustom.ini") >= 0) {
			read();
		}
	}

	bool Allows(const RE::TESForm& a_form)
	{
		switch (a_form.GetFormType()) {
		case RE::FormType::kWEAP:
			return weapons;
		case RE::FormType::kARMO:
			return armor;
		default:
			return false;
		}
	}

	bool Powers()
	{
		return powers;
	}
}

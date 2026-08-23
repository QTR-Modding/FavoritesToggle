#pragma once

namespace RE
{
	class TESForm;
}

namespace FavoritesToggle::Settings
{
	void               Load();
	[[nodiscard]] bool Allows(const RE::TESForm& a_form);
	[[nodiscard]] bool Powers();
}

namespace FavoritesToggle::Vanilla
{
	[[nodiscard]] bool Install();
}

namespace FavoritesToggle::FME
{
	[[nodiscard]] bool Install();
}

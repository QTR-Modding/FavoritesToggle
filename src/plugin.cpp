#include "Hooks.h"

namespace FavoritesToggle
{
	// ReSharper disable once CppParameterMayBeConstPtrOrRef
	// ReSharper disable once CppUseInternalLinkage
	void OnPostLoad(SFSE::MessagingInterface::Message* a_message) noexcept
	{
		if (!a_message || a_message->type != SFSE::MessagingInterface::kPostLoad) {
			return;
		}

		try {
			if (!Vanilla::Install()) {
				return;
			}
		} catch (const std::exception& error) {
			logger::error("Could not install vanilla favorites hooks: {}", error.what());
			return;
		}

		try {
			if (!FME::Install()) {
				logger::warn("Favorites Menu Extended compatibility is inactive");
			}
		} catch (const std::exception& error) {
			logger::warn("Could not install Favorites Menu Extended compatibility: {}", error.what());
		}
	}
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
	if (!a_sfse) {
		return false;
	}

	constexpr SFSE::InitInfo initInfo{
		.logPattern = "%Y-%m-%d %H:%M:%S.%e [%l] %v",
		.trampoline = true,
		.trampolineSize = 2 * sizeof(REL::ASM::JMP14)
	};
	SFSE::Init(a_sfse, initInfo);

	const auto runtime = a_sfse->RuntimeVersion();
	logger::info(
		"{} {} loaded; runtime={}, SFSE=0x{:08X}",
		SFSE::GetPluginName(),
		SFSE::GetPluginVersion(),
		runtime,
		a_sfse->SFSEVersion());
	if (runtime != SFSE::RUNTIME_SF_1_16_244) {
		logger::error("Unsupported runtime {}; expected {}", runtime, SFSE::RUNTIME_SF_1_16_244);
		return false;
	}

	FavoritesToggle::Settings::Load();

	const auto messaging = SFSE::GetMessagingInterface();
	if (!messaging) {
		logger::error("Required SFSE MessagingInterface is unavailable");
		return false;
	}
	if (!messaging->RegisterListener(FavoritesToggle::OnPostLoad)) {
		logger::error("Could not register the SFSE message listener");
		return false;
	}

	return true;
}

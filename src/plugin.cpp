#include "Hooks.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

namespace FavoritesToggle
{
    void InitializeLog();
}

void FavoritesToggle::InitializeLog()
{
    try {
        if (const auto directory = SFSE::GetLogDirectory()) {
            const auto logFile = *directory / (PLUGIN_NAME ".log");
            const auto log = logger::basic_logger_mt(PLUGIN_NAME, logFile.string(), true);
            log->sinks().push_back(std::make_shared<logger::sinks::msvc_sink_mt>());
#ifdef NDEBUG
            constexpr auto level = logger::level::info;
#else
            constexpr auto level = logger::level::debug;
#endif
            log->set_level(level);
            log->flush_on(level);
            log->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
            logger::set_default_logger(log);
        }
    } catch (const logger::spdlog_ex& error) {
        logger::warn("Could not start the file log: {}", error.what());
    }
}

namespace FavoritesToggle
{
	namespace
	{
		// ReSharper disable once CppParameterMayBeConstPtrOrRef
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
}

extern "C"
{
    __declspec(dllexport) constinit SFSE::PluginVersionData SFSEPlugin_Version{
        1,
        PLUGIN_VERSION_PACKED,
        PLUGIN_NAME,
        PLUGIN_AUTHOR,
        1U << 2,
        1U << 3,
        { SFSE::RUNTIME_SF_1_16_244 },
        0,
        0,
        0
    };
}

extern "C" __declspec(dllexport) bool SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
	if (!a_sfse) {
		return false;
	}

	FavoritesToggle::InitializeLog();
	const auto runtime = a_sfse->RuntimeVersion();
	if (runtime != SFSE::RUNTIME_SF_1_16_244) {
		logger::error("Unsupported runtime 0x{:08X}; expected Starfield 1.16.244", runtime);
		return false;
	}
	if (!SFSE::Init(*a_sfse, 2 * sizeof(REL::ASM::JMP14))) {
		logger::error("Could not initialize SFSE");
		return false;
	}

	logger::info(
		"{} {} loaded; runtime=0x{:08X}, SFSE=0x{:08X}",
		PLUGIN_NAME,
		PLUGIN_VERSION_TEXT,
		runtime,
		a_sfse->sfseVersion);

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

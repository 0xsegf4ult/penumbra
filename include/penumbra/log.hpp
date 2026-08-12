#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace penumbra
{

inline void log_init()
{
        auto console = spdlog::stdout_color_mt("penumbra");

        console->set_pattern("[%^%l%$] %v");
        console->set_level(spdlog::level::debug);

        spdlog::set_default_logger(console);
}

namespace log
{
	using spdlog::debug;
	using spdlog::info;
	using spdlog::warn;
	using spdlog::error;
	using spdlog::critical;
}

}

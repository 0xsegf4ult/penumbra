#pragma once

#include <penumbra/array_proxy.hpp>
#include <string_view>

namespace penumbra
{

using cmd_args_t = array_proxy<std::string_view>;

struct cmd_t
{
	std::string_view name;
	void (*callback)(cmd_args_t args);
	cmd_t* next{nullptr};
};

void cmd_register(cmd_t* cmd);
cmd_t* cmd_get(std::string_view name);

typedef void (*cmd_sink_callback)(std::string_view data);

void cmd_executestring(std::string_view string);
void cmd_register_output_sink(cmd_sink_callback callback);

}

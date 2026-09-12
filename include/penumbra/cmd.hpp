#pragma once

#include <string_view>

namespace penumbra
{

typedef void (*cmd_sink_callback)(std::string_view data);

void cmd_executestring(std::string_view string);
void cmd_register_output_sink(cmd_sink_callback callback);

}

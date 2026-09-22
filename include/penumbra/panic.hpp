#pragma once

#include <string_view>

namespace penumbra
{

[[noreturn]] void panic(std::string_view message);

}

#pragma once

#include <penumbra/resource/rid.hpp>

namespace penumbra
{

struct animation_component
{
	ResourceID animation;

	float cur_time{0.0f};
	bool running{true};
	bool loop{true};
};

}

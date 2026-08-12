#pragma once

#include <penumbra/math/vector.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct directional_light_component
{
	vec3 direction;
	vec3 color;
	float intensity;
};

struct point_light_component
{
	vec3 color{1.0f};
	float intensity{1500.0f};
	float radius{5.0f};
	u32 offset{0u};
	bool shadowcast{true};
};

struct spotlight_component
{
	vec3 direction{0.0f, 0.0f, -1.0f};
	vec3 color{1.0f};
	float intensity{1500.0f};
	float range{16.0f};
	float inner_cone{42.5f};
	float outer_cone{45.0f};
	u32 offset{0u};
	bool shadowcast{true};
};

}

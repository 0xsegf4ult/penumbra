export module penumbra.editor:particle_components;

import penumbra.math;
import std;

using std::uint32_t;

namespace penumbra
{

export struct emitter_component
{
	uint32_t renderer_objectID;

	vec3 initial_force{0.0f};
	float drag{1.0f};
	vec3 initial_velocity{0.0f};
	float life{1.0f};
	float size{0.01f};
	float size_scaling{1.2f};
	float random_factor{1.0f};
	float life_random_factor{1.0f};
	uint32_t flags;

	bool enabled{true};
};

export struct forcefield_component
{
	float gravity{0.0f};
	float range{10.0f};
};

}

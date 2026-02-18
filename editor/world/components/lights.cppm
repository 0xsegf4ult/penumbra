export module penumbra.editor:light_components;

import penumbra.math;


namespace penumbra
{

export struct directional_light_component
{
	vec3 direction;
	vec3 color;
	float intensity;
};

export struct point_light_component
{
	vec3 color{1.0f};
	float intensity{1500.0f};
	float radius{5.0f};
	bool shadowcast{true};
};

export struct spotlight_component
{
	vec3 direction{0.0f, 0.0f, -1.0f};
	vec3 color{1.0f};
	float intensity{1500.0f};
	float range{16.0f};
	float inner_cone{42.5f};
	float outer_cone{45.0f};
	bool shadowcast{true};
};

}

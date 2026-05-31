export module penumbra.editor:animation_component;

import penumbra.math;
import penumbra.resource;

namespace penumbra
{

export struct animation_component
{
	ResourceID animation;

	float cur_time{0.0f};
	bool running{true};
	bool loop{true};
};

}

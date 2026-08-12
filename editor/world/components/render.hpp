#pragma once

#include <penumbra/resource/rid.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct render_object_component
{
	ResourceID geometry;
	ResourceID material;
	u32 renderer_objectID;
};

struct render_skeleton_component
{
	ResourceID skeleton;
};

}

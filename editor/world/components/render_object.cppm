export module penumbra.editor:render_object_component;

import penumbra.renderer;
import penumbra.resource;

import std;

namespace penumbra
{

export struct render_object_component
{
	ResourceID geometry;
	ResourceID material;
	RenderObject renderer_objectID;
};

export struct render_anim_component
{
	ResourceID skeleton;
	std::uint32_t sg_instance;
};

}

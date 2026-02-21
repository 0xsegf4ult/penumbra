export module penumbra.editor:render_object_component;

import penumbra.renderer;
import penumbra.resource;

namespace penumbra
{

export struct render_object_component
{
	ResourceID geometry;
	ResourceID material;
	RenderObject renderer_objectID;
};

}

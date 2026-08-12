#pragma once

#include <penumbra/renderer.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct render_bucket_draw
{
	GPUPointer commands;
	GPUPointer counter;
	GPUPointer instances;
	u32 max_instance_count;
};

void renderer_world_init();
void renderer_world_cleanup();
void renderer_world_update(GPUCommandBuffer& cmd);
void renderer_world_determine_visibility(GPUCommandBuffer& cmd);
renderObjectID renderer_world_insert_object_internal(const render_object_desc& desc, array_proxy<renderViewID> views);
GPUPointer renderer_world_get_objects();
render_bucket_draw renderer_world_get_drawcall(renderViewID view, render_bucket bucket);

}

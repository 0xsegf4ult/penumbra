#pragma once

#include <penumbra/renderer.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct render_bucket_draw
{
	GPUPointer commands;
	GPUPointer instances;
};

struct render_bucket_mdi_draw
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
GPUPointer renderer_world_get_objects();
GPUPointer renderer_world_get_lights();
u32 renderer_world_get_light_count();
render_bucket_draw renderer_world_get_drawcall(renderViewID view, render_bucket bucket);
render_bucket_mdi_draw renderer_world_get_mdi_drawcall(renderViewID view, render_bucket bucket);

}

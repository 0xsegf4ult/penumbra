#include <renderer/transparent.hpp>
#include <renderer/resource.hpp>
#include <renderer/visbuffer.hpp>
#include <renderer/world.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

static GPUPipeline transparent_pbr_pso;
static GPUDepthStencilDesc reverse_z_transparent;

void renderer_transparent_init()
{
	GPUBlendDesc alpha_blend_state
	{
		.src_color_factor = GPU_BLEND_FACTOR_ONE,
		.dst_color_factor = GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, 
		.src_alpha_factor = GPU_BLEND_FACTOR_ONE,
		.dst_alpha_factor = GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
	};	

	transparent_pbr_pso = gpu_create_graphics_pipeline(load_shader("shaders/transparent_PBR"),
	{
		.color_targets = {GPU_FORMAT_B10GR11_UFLOAT},
		.depth_format = GPU_FORMAT_D32_SFLOAT,
		.blendstate = &alpha_blend_state
	});

	reverse_z_transparent = GPUDepthStencilDesc
	{
		.depth_mode = GPU_DEPTH_READ,
		.depth_test = GPU_COMPARE_OP_GREATER
	};	
}

void renderer_transparent_cleanup()
{
	gpu_destroy_pipeline(transparent_pbr_pso);
}

void renderer_transparent_draw(visibility_buffer& visbuffer, GPUCommandBuffer& cmd)
{
	struct TransparentPSOData
	{
		GPUDevicePointer objects;
		GPUDevicePointer instances;
		GPUDevicePointer materials;
	} shader_data;
	shader_data.objects = gpu_host_to_device_pointer(renderer_world_get_objects());
	shader_data.materials = gpu_host_to_device_pointer(renderer_material_get_storage());
	
	gpu_set_pipeline(cmd, transparent_pbr_pso);
	gpu_set_cullmode(cmd, GPU_CULLMODE_CW);
	gpu_set_depth_stencil_state(cmd, reverse_z_transparent);
	gpu_write_cbuffer_descriptor(cmd, visbuffer.cbuffer[renderer_gfx_frame_index()]);
	gpu_bind_index_buffer(cmd, renderer_geometry_get_storage().index, GPU_INDEX_TYPE_U8);

	auto drawcall = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_TRANSPARENT);
	shader_data.instances = gpu_host_to_device_pointer(drawcall.instances);
	gpu_draw_indexed_indirect_count(cmd, &shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);

	gpu_set_cullmode(cmd, GPU_CULLMODE_NONE);
	drawcall = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_TRANSPARENT_DOUBLE_SIDED);
	gpu_draw_indexed_indirect_count(cmd, &shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);
}

}


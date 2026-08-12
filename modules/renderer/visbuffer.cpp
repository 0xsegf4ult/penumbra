#include <renderer/visbuffer.hpp>
#include <renderer/resource.hpp>
#include <renderer/world.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/config.hpp>
#include <penumbra/types.hpp>
#include <penumbra/renderer.hpp>

namespace penumbra
{

static GPUPipeline visbuffer_build_pso;
static GPUPipeline visbuffer_build_alphamask_pso;
static GPUPipeline visbuffer_visualize_cs;
static GPUPipeline visbuffer_resolve_cs;
static GPUDepthStencilDesc vb_ds_reverse_z;

void renderer_visbuffer_init(visibility_buffer& visbuffer)
{
	visbuffer_build_pso = gpu_create_graphics_pipeline(load_shader("shaders/visbuffer_build_opaque"),
	{
		.color_targets = {GPU_FORMAT_R32_UINT},
		.depth_format = GPU_FORMAT_D32_SFLOAT
	});

	visbuffer_build_alphamask_pso = gpu_create_graphics_pipeline(load_shader("shaders/visbuffer_build_alphamask"),
	{
		.color_targets = {GPU_FORMAT_R32_UINT},
		.depth_format = GPU_FORMAT_D32_SFLOAT
	});

	visbuffer_visualize_cs = gpu_create_compute_pipeline(load_shader("shaders/visbuffer_visualize"));
	visbuffer_resolve_cs = gpu_create_compute_pipeline(load_shader("shaders/visbuffer_resolve"));

	vb_ds_reverse_z = GPUDepthStencilDesc
	{
		.depth_mode = GPU_DEPTH_READ | GPU_DEPTH_WRITE,
		.depth_test = GPU_COMPARE_OP_GREATER
	};
	
	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		visbuffer.cbuffer[i] = gpu_allocate_memory(sizeof(visbuffer_cbuffer), GPU_MEMORY_MAPPED, GPU_BUFFER_UNIFORM);
	}
}

void renderer_visbuffer_cleanup(visibility_buffer& visbuffer)
{
	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		gpu_free_memory(visbuffer.cbuffer[i]);
	}
	
	gpu_destroy_pipeline(visbuffer_resolve_cs);
	gpu_destroy_pipeline(visbuffer_visualize_cs);
	gpu_destroy_pipeline(visbuffer_build_alphamask_pso);
	gpu_destroy_pipeline(visbuffer_build_pso);
}

void renderer_visbuffer_init_rendertarget(visibility_buffer& visbuffer, GPUCommandBuffer& cmd, uvec2 resolution)
{
	visbuffer.resolution = resolution;
	visbuffer.texture = gpu_create_texture
	({
		.dim = uvec3{resolution, 1u},
		.format = GPU_FORMAT_R32_UINT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_COLOR_ATTACHMENT
	});

	visbuffer.descriptor = gpu_texture_view_descriptor(visbuffer.texture, {.format = GPU_FORMAT_R32_UINT});

	gpu_texture_layout_transition(cmd, visbuffer.texture, GPU_STAGE_NONE, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
}

void renderer_visbuffer_cleanup_rendertarget(visibility_buffer& visbuffer)
{
	gpu_destroy_texture(visbuffer.texture);
	gpu_free_descriptor(visbuffer.descriptor);	
}

void renderer_visbuffer_build(visibility_buffer& visbuffer, GPUCommandBuffer& cmd, GPUTexture zbuffer)
{
	auto geometry_storage = renderer_geometry_get_storage();

	struct VBBuildData
	{
		GPUDevicePointer objects;
		GPUDevicePointer instances;
	} shader_data;

	struct VBBuildAlphaData
	{
		GPUDevicePointer objects;
		GPUDevicePointer instances;
		GPUDevicePointer materials;
	} am_shader_data;
	
	shader_data.objects = gpu_host_to_device_pointer(renderer_world_get_objects());

	am_shader_data.objects = shader_data.objects;
	am_shader_data.materials = gpu_host_to_device_pointer(renderer_material_get_storage());

	gpu_begin_renderpass(cmd,
	{
		.color_targets =
		{
			{
			.texture = visbuffer.texture,
			.load_op = GPU_LOAD_OP_CLEAR
			}
		},
		.depth_target =
		{
			.texture = zbuffer,
			.load_op = GPU_LOAD_OP_CLEAR
		}
	});

	gpu_set_pipeline(cmd, visbuffer_build_pso);
	gpu_set_cullmode(cmd, GPU_CULLMODE_CW);
	gpu_set_depth_stencil_state(cmd, vb_ds_reverse_z);
	gpu_write_cbuffer_descriptor(cmd, visbuffer.cbuffer[renderer_gfx_frame_index()]);

	gpu_bind_index_buffer(cmd, geometry_storage.index, GPU_INDEX_TYPE_U8);
	auto drawcall = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_DEFAULT);
	shader_data.instances = gpu_host_to_device_pointer(drawcall.instances);
	gpu_draw_indexed_indirect_count(cmd, &shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);

	gpu_set_cullmode(cmd, GPU_CULLMODE_NONE);
	drawcall = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_DOUBLE_SIDED);
	gpu_draw_indexed_indirect_count(cmd, &shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);

	gpu_set_pipeline(cmd, visbuffer_build_alphamask_pso);
	gpu_set_depth_stencil_state(cmd, vb_ds_reverse_z);
	gpu_write_cbuffer_descriptor(cmd, visbuffer.cbuffer[renderer_gfx_frame_index()]);

	drawcall = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_ALPHA_MASKED_DOUBLE_SIDED);
	am_shader_data.instances = gpu_host_to_device_pointer(drawcall.instances);
	gpu_draw_indexed_indirect_count(cmd, &am_shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);

	gpu_set_cullmode(cmd, GPU_CULLMODE_CW);
	drawcall = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_ALPHA_MASKED);
	gpu_draw_indexed_indirect_count(cmd, &am_shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);
	
	gpu_end_renderpass(cmd);
}

void renderer_visbuffer_visualize(visibility_buffer& visbuffer, GPUCommandBuffer& cmd, GPUTextureDescriptor& output)
{
	gpu_set_pipeline(cmd, visbuffer_visualize_cs);

	struct VBVisualizeData
	{
		GPUDevicePointer instances;
		uvec2 res;
		u32 visbuffer;
		u32 output;
	} shader_data;

	auto drawdata = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_DEFAULT);
	shader_data.instances = gpu_host_to_device_pointer(drawdata.instances);
	shader_data.res = visbuffer.resolution;
	shader_data.visbuffer = visbuffer.descriptor.handle;
	shader_data.output = output.handle;
	gpu_dispatch(cmd, &shader_data, {(visbuffer.resolution.x + 7u) / 8u, (visbuffer.resolution.y + 7u) / 8u, 1u});
}

void renderer_visbuffer_resolve(visibility_buffer& visbuffer, GPUCommandBuffer& cmd, GPUTextureDescriptor& hdr_output)
{
	gpu_set_pipeline(cmd, visbuffer_resolve_cs);
	gpu_write_cbuffer_descriptor(cmd, visbuffer.cbuffer[renderer_gfx_frame_index()]);

	struct VBResolveData
	{
		GPUDevicePointer instances;
		GPUDevicePointer objects;
		GPUDevicePointer materials;
		GPUDevicePointer clusters;
		u32 visbuffer;
		u32 output;
	} shader_data;

	auto drawdata = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_DEFAULT);

	shader_data.instances = gpu_host_to_device_pointer(drawdata.instances);
       	shader_data.objects = gpu_host_to_device_pointer(renderer_world_get_objects());
	shader_data.materials = gpu_host_to_device_pointer(renderer_material_get_storage());
	shader_data.clusters = gpu_host_to_device_pointer(renderer_geometry_get_storage().cluster);
	shader_data.visbuffer = visbuffer.descriptor.handle;
	shader_data.output = hdr_output.handle;

	gpu_dispatch(cmd, &shader_data, {(visbuffer.resolution.x + 7u) / 8u, (visbuffer.resolution.y + 7u) / 8u, 1u});
}	

}

module;

#include <cassert>
#include <tracy/Tracy.hpp>

module penumbra.renderer;
import :geometry_buffer;
import :render_world;
import :material;

import penumbra.core;
import penumbra.gpu;
import penumbra.ui;

import std;
using std::uint64_t, std::size_t;

namespace penumbra
{

struct StreamBuffer
{
	constexpr static size_t chunk_size = 32 * 1024 * 1024; 
	struct Chunk
	{
		GPUPointer data;
		uint32_t head{0u};
		uint64_t syncval{0};
	};

	std::vector<Chunk> chunks;
};

struct TextureWriteRequest
{
	GPUPointer data;
	GPUTexture texture;
	uint32_t num_mips;
	uint32_t num_layers;
};

struct VisbufferCBuffer
{
	mat4 camera;
	mat4 inverse_projection;
	mat4 inverse_view;

	GPUDevicePointer vertex_pos;
	GPUDevicePointer vertex_uv;
	GPUDevicePointer vertex_nor_tan;
	GPUDevicePointer geom_indices;
	vec2 res;
	vec2 inv_res;
	vec4 light_direction;
	vec4 light_color;
	float exposure;
};

struct renderer_context_t
{
	Window* window;

	std::array<uint64_t, config::renderer_frames_in_flight> gfx_queue_frames;
	std::array<uint64_t, config::renderer_frames_in_flight> compute_queue_frames;
	int frame_index;

	std::array<GPUSemaphore, config::renderer_frames_in_flight> swapchain_acquire;
	std::array<GPUSemaphore, config::renderer_frames_in_flight> swapchain_present;
	GPUTexture cur_swapchain;

	GPUSemaphore transfer_resource_semaphore;
	uint64_t transfer_resource_sync{0};

	RenderWorld render_world;
	RenderView camera_view;

	std::array<GPUPointer, config::renderer_frames_in_flight> visbuffer_cbv;

	uvec2 last_render_resolution{800u, 600u};
	uvec2 render_resolution{800u, 600u};

	RenderMaterialStorage materials;
	StreamBuffer stream_buffer;
	std::vector<TextureWriteRequest> texwrites;

	GPUTexture visbuffer_tex;
	GPUTexture depthbuffer_tex;
	GPUTexture framebuffer_tex;
	GPUTextureDescriptor visbuffer;
	GPUTextureDescriptor depthbuffer;
	GPUTextureDescriptor framebuffer;
	GPUTextureDescriptor framebuffer_rw;

	GPUPipeline visbuffer_build_pso;
	GPUPipeline vb_resolve_cs;

	std::vector<visbuffer_read_hook> visbuffer_read_hooks;
};

renderer_context_t* renderer = nullptr;

void renderer_create_rendertargets()
{
	renderer->visbuffer_tex = gpu_create_texture
	({
		.dim = uvec3{renderer->render_resolution, 1u},
		.format = GPU_FORMAT_R32_UINT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_COLOR_ATTACHMENT
	});
	renderer->visbuffer = gpu_texture_view_descriptor(renderer->visbuffer_tex, {.format = GPU_FORMAT_R32_UINT});

	renderer->depthbuffer_tex = gpu_create_texture
	({
		.dim = uvec3{renderer->render_resolution, 1u},
		.format = GPU_FORMAT_D32_SFLOAT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_DEPTH_STENCIL_ATTACHMENT
	});
	renderer->depthbuffer = gpu_texture_view_descriptor(renderer->depthbuffer_tex, {.format = GPU_FORMAT_D32_SFLOAT});

	renderer->framebuffer_tex = gpu_create_texture
	({
		.dim = uvec3{renderer->render_resolution, 1u},
		.format = GPU_FORMAT_RGBA8_UNORM,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_STORAGE
	});
	renderer->framebuffer_rw = gpu_rwtexture_view_descriptor(renderer->framebuffer_tex, {.format = GPU_FORMAT_RGBA8_UNORM});
	renderer->framebuffer = gpu_texture_view_descriptor(renderer->framebuffer_tex, {.format = GPU_FORMAT_RGBA8_UNORM});

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_texture_layout_transition(cmd, renderer->visbuffer_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_texture_layout_transition(cmd, renderer->depthbuffer_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_texture_layout_transition(cmd, renderer->framebuffer_tex, GPU_STAGE_NONE, GPU_STAGE_COMPUTE, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
}

void renderer_init(Window& wnd)
{
	renderer = new renderer_context_t();
	renderer->window = &wnd;

	gpu_init();
	gpu_swapchain_init(wnd);
	renderer->frame_index = 0;
	
	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		renderer->gfx_queue_frames[i] = 0;
		renderer->compute_queue_frames[i] = 0;

		renderer->swapchain_acquire[i] = gpu_create_semaphore(0, GPU_SEMAPHORE_BINARY);
		renderer->swapchain_present[i] = gpu_create_semaphore(0, GPU_SEMAPHORE_BINARY);
	}

	renderer->transfer_resource_semaphore = gpu_create_semaphore(0);
	
	imgui_backend_init(renderer->window);

	renderer_geometry_init();

	renderer->render_world.init();
	renderer->camera_view = renderer->render_world.register_view();

	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		renderer->visbuffer_cbv[i] = gpu_allocate_memory(sizeof(VisbufferCBuffer), GPU_MEMORY_MAPPED, GPU_BUFFER_UNIFORM);

		VisbufferCBuffer* vbconst = reinterpret_cast<VisbufferCBuffer*>(gpu_map_memory(renderer->visbuffer_cbv[i]));
		vbconst->vertex_pos = renderer_geometry_vertex_pos_device_pointer();
		vbconst->vertex_uv = renderer_geometry_vertex_uv_device_pointer();
		vbconst->vertex_nor_tan = renderer_geometry_vertex_nor_tan_device_pointer();
		vbconst->geom_indices = renderer_geometry_index_device_pointer();
		vbconst->light_direction = vec4{-0.14f, -0.3f, -0.3f, 0.0f};
		vbconst->light_color = vec4{0.68f * 38000.0f, 0.53f * 38000.0f, 0.46f * 38000.0f, 0.0f};
	}

	renderer->stream_buffer.chunks.push_back({gpu_allocate_memory(StreamBuffer::chunk_size, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD), 0u});

	gpu_create_sampler
	({
		.mag_filter = GPU_FILTER_LINEAR,
		.min_filter = GPU_FILTER_LINEAR,
		.mip_filter = GPU_FILTER_LINEAR,
		.address_mode_u = GPU_ADDRESS_MODE_REPEAT,
		.address_mode_v = GPU_ADDRESS_MODE_REPEAT,
		.address_mode_w = GPU_ADDRESS_MODE_REPEAT,
		.max_anisotropy = 4.0f
	});
	renderer->materials.data = gpu_allocate_memory(renderer->materials.capacity * sizeof(RenderMaterialData), GPU_MEMORY_MAPPED);
	log::debug("material buffer {:x}", gpu_host_to_device_pointer(renderer->materials.data));
	renderer_write_material(RenderMaterialData{});

	renderer_create_rendertargets();

	renderer->visbuffer_build_pso = gpu_create_graphics_pipeline(*load_shader("shaders/visbuffer_build_opaque"), 	
	{
		.color_targets = {GPU_FORMAT_R32_UINT},
		.depth_format = GPU_FORMAT_D32_SFLOAT
	});

	renderer->vb_resolve_cs = gpu_create_compute_pipeline(*load_shader("shaders/visbuffer_resolve"));
}

void renderer_shutdown()
{
	gpu_wait_idle();

	gpu_destroy_pipeline(renderer->vb_resolve_cs);
	gpu_destroy_pipeline(renderer->visbuffer_build_pso);

	gpu_destroy_texture(renderer->framebuffer_tex);
	gpu_destroy_texture(renderer->depthbuffer_tex);
	gpu_destroy_texture(renderer->visbuffer_tex);

	gpu_free_memory(renderer->materials.data);

	for(auto& chunk : renderer->stream_buffer.chunks)
		gpu_free_memory(chunk.data);

	for(auto& cbv : renderer->visbuffer_cbv)
		gpu_free_memory(cbv);
		
	renderer_geometry_shutdown();

	imgui_backend_shutdown();

	gpu_destroy_semaphore(renderer->transfer_resource_semaphore);

	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		gpu_destroy_semaphore(renderer->swapchain_acquire[i]);
		gpu_destroy_semaphore(renderer->swapchain_present[i]);
	}

	delete renderer;
	
	gpu_shutdown();
}

void renderer_next_frame()
{
	ZoneScoped;
	
	if(renderer->render_resolution != renderer->last_render_resolution)
	{
		renderer->last_render_resolution = renderer->render_resolution;
		gpu_wait_queue(GPU_QUEUE_GRAPHICS, renderer->gfx_queue_frames[renderer->frame_index]);

		gpu_destroy_texture(renderer->framebuffer_tex);
		gpu_destroy_texture(renderer->depthbuffer_tex);
		gpu_destroy_texture(renderer->visbuffer_tex);

		gpu_free_descriptor(renderer->framebuffer);
		gpu_free_descriptor(renderer->depthbuffer);
		gpu_free_descriptor(renderer->visbuffer);

		renderer_create_rendertargets();
	}

	renderer->frame_index = (renderer->frame_index + 1) % config::renderer_frames_in_flight;
	if(!gpu_wait_queue(GPU_QUEUE_GRAPHICS, renderer->gfx_queue_frames[renderer->frame_index]))
		panic("renderer: gfx queue stuck!");

	gpu_wait_queue(GPU_QUEUE_COMPUTE, renderer->compute_queue_frames[renderer->frame_index]);
	renderer->cur_swapchain = gpu_swapchain_acquire_next(renderer->swapchain_acquire[renderer->frame_index]);
}

void renderer_copy_resources_async()
{
	auto transfer_time = gpu_semaphore_read_counter(renderer->transfer_resource_semaphore);
	for(auto& chunk : renderer->stream_buffer.chunks)
	{
		if(chunk.syncval <= transfer_time)
			chunk.head = 0;
	}

	if(!renderer_geometry_needs_upload() && renderer->texwrites.empty())
		return;

	auto cmd = gpu_record_commands(GPU_QUEUE_TRANSFER);
	
	if(renderer_geometry_needs_upload())
	{
		renderer_geometry_copy_async(cmd);
	}

	for(auto& write : renderer->texwrites)
	{
		gpu_texture_layout_transition(cmd, write.texture, GPU_STAGE_NONE, GPU_STAGE_TRANSFER, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
		gpu_copy_to_texture(cmd, write.data, write.texture, write.num_mips);
	}
	renderer->texwrites.clear();

	renderer->transfer_resource_sync++;
	gpu_barrier(cmd, GPU_STAGE_TRANSFER, GPU_STAGE_ALL);
	gpu_emit_signal(cmd, GPU_STAGE_ALL, renderer->transfer_resource_semaphore, renderer->transfer_resource_sync);
	auto transfer_sync = gpu_submit(GPU_QUEUE_TRANSFER, cmd);
}

void renderer_process_frame(double dt)
{
	ZoneScoped;
	renderer_copy_resources_async();
	
	assert(renderer->cur_swapchain);
	assert(renderer->cur_swapchain->texture);
	
	vec2 f_res{static_cast<float>(renderer->render_resolution.x), static_cast<float>(renderer->render_resolution.y)};
	VisbufferCBuffer* vbconst = reinterpret_cast<VisbufferCBuffer*>(gpu_map_memory(renderer->visbuffer_cbv[renderer->frame_index]));
	vbconst->res = f_res;
	vbconst->inv_res = vec2{1.0f / f_res.x, 1.0f / f_res.y};
	
	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_wait_signal(cmd, GPU_STAGE_RASTER_OUTPUT, renderer->swapchain_acquire[renderer->frame_index], 0);
	gpu_texture_layout_transition(cmd, renderer->cur_swapchain, GPU_STAGE_RASTER_OUTPUT, GPU_STAGE_RASTER_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	renderer->render_world.upload_objects(cmd);
	renderer->render_world.determine_visibility(cmd);

	gpu_begin_renderpass(cmd,
	{
		.color_targets =
		{
			{
			.texture = renderer->visbuffer_tex, 
			.load_op = GPU_LOAD_OP_CLEAR
			}
		},
		.depth_target = 
		{
			.texture  = renderer->depthbuffer_tex,
			.load_op = GPU_LOAD_OP_CLEAR
		}
	});

	gpu_set_pipeline(cmd, renderer->visbuffer_build_pso);
	
	gpu_set_cull_mode(cmd, GPU_CULLMODE_CW);

	GPUDepthStencilDesc reverse_z
	{
		.depth_mode = GPU_DEPTH_READ | GPU_DEPTH_WRITE,
		.depth_test = GPU_OP_GREATER
	};
	gpu_set_depth_stencil_state(cmd, reverse_z);

	gpu_write_cbuffer_descriptor(cmd, renderer->visbuffer_cbv[renderer->frame_index]);

	auto draw_data = renderer_world_get_bucket(renderer->camera_view, RENDER_BUCKET_DEFAULT);
	
	struct VBBuildData
	{
		GPUDevicePointer objects;
		GPUDevicePointer instances;
	} shader_data;
	shader_data.objects = renderer->render_world.get_objects();
	shader_data.instances = gpu_host_to_device_pointer(draw_data.instances);

	gpu_bind_index_buffer(cmd, renderer_geometry_index_pointer(), GPU_INDEX_TYPE_U8);
	gpu_draw_indexed_indirect_count(cmd, &shader_data, draw_data.commands, draw_data.counter, draw_data.max_instance_count);
	gpu_end_renderpass(cmd);

	gpu_barrier(cmd, GPU_STAGE_RASTER_OUTPUT, GPU_STAGE_COMPUTE);

	for(auto& hook : renderer->visbuffer_read_hooks)
		hook(cmd, &renderer->visbuffer, renderer->render_resolution, renderer->frame_index);

	gpu_set_pipeline(cmd, renderer->vb_resolve_cs);

	gpu_write_cbuffer_descriptor(cmd, renderer->visbuffer_cbv[renderer->frame_index]);

	struct VBResolveData
	{
		GPUDevicePointer instances;
		GPUDevicePointer objects;
		GPUDevicePointer materials;
		GPUDevicePointer clusters;
		uint32_t visbuffer;
		uint32_t output;	
	} vbr_data;
	vbr_data.instances = shader_data.instances;
	vbr_data.objects = renderer->render_world.get_objects();
	vbr_data.materials = gpu_host_to_device_pointer(renderer->materials.data);
	vbr_data.clusters = renderer_geometry_cluster_device_pointer();
	vbr_data.visbuffer = renderer->visbuffer.handle;
	vbr_data.output = renderer->framebuffer_rw.handle;
	gpu_dispatch(cmd, &vbr_data, {(renderer->render_resolution.x + 7u) / 8u, (renderer->render_resolution.y + 7u) / 8u, 1u});

	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_FRAGMENT_SHADER);

	gpu_begin_renderpass(cmd,
	{
		.color_targets =
		{
			{
			.texture = renderer->cur_swapchain, 
			.load_op = GPU_LOAD_OP_CLEAR
			}
		}
	});

	imgui_backend_render(cmd, dt);

	gpu_end_renderpass(cmd);

	gpu_texture_layout_transition(cmd, renderer->cur_swapchain, GPU_STAGE_RASTER_OUTPUT, GPU_STAGE_ALL, GPU_TEXTURE_LAYOUT_GENERAL, GPU_TEXTURE_LAYOUT_PRESENT);
	gpu_emit_signal(cmd, GPU_STAGE_ALL, renderer->swapchain_present[renderer->frame_index], 0);
	auto gfx_sync = gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	renderer->gfx_queue_frames[renderer->frame_index] = gfx_sync;

	gpu_swapchain_present(GPU_QUEUE_GRAPHICS, renderer->swapchain_present[renderer->frame_index]);
}

uint32_t renderer_gfx_frame_index()
{
	return renderer->frame_index;
}

uint64_t renderer_resource_transfer_syncval()
{
	return renderer->transfer_resource_sync;
}

StreamBuffer::Chunk& stream_buffer_acquire(size_t size)
{
	if(size > StreamBuffer::chunk_size)
	{
		log::error("stream_buffer_acquire: data {} larger than chunk size {}", size, StreamBuffer::chunk_size);
		std::abort();
	}

	for(auto& chunk : renderer->stream_buffer.chunks)
	{	
		if(chunk.head + size <= StreamBuffer::chunk_size)
			return chunk;
	}

	renderer->stream_buffer.chunks.push_back({gpu_allocate_memory(StreamBuffer::chunk_size, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD), 0u});
	return renderer->stream_buffer.chunks.back();
}

void renderer_write_texture(GPUTexture texture, std::span<const std::byte> data, uint32_t num_mips, uint32_t num_layers)
{
	auto& stream_block = stream_buffer_acquire(data.size());
	memcpy(gpu_map_memory(stream_block.data + stream_block.head), data.data(), data.size());
	renderer->texwrites.push_back({stream_block.data + stream_block.head, texture, num_mips, num_layers});
	stream_block.head += data.size();
	stream_block.syncval = renderer->transfer_resource_sync + 1;
}

void renderer_write_material(const RenderMaterialData& data)
{
	if(renderer->materials.size >= renderer->materials.capacity)
	{
		log::warn("renderer_write_material: out of material storage memory [{}]", renderer->materials.capacity);
		return;
	}

	memcpy(gpu_map_memory(renderer->materials.data) + (renderer->materials.size * sizeof(RenderMaterialData)), &data, sizeof(RenderMaterialData));
	renderer->materials.size++;
}

GPUDevicePointer renderer_materials_device_pointer()
{
	return gpu_host_to_device_pointer(renderer->materials.data);
}

RenderObject renderer_world_insert_object(const RenderObjectDescription& data)
{
	return renderer->render_world.insert_object(data, {renderer->camera_view});
}

RenderBucketData renderer_world_get_bucket(RenderView view, RenderBucket bucket)
{
	return renderer->render_world.get_bucket(view, bucket);
}

GPUTextureDescriptor* renderer_get_framebuffer()
{
	return &renderer->framebuffer;
}

uvec2 renderer_get_render_resolution()
{
	return renderer->render_resolution;
}

void renderer_update_render_resolution(uvec2 res)
{
	renderer->render_resolution = res;
}

void renderer_update_camera(const mat4& view, const mat4& proj, float exposure)
{
	VisbufferCBuffer* vbconst = reinterpret_cast<VisbufferCBuffer*>(gpu_map_memory(renderer->visbuffer_cbv[renderer->frame_index]));
	vbconst->camera = view * proj;
	vbconst->inverse_projection = mat4::inverse(proj);
	vbconst->inverse_view = mat4::inverse(view);
	vbconst->exposure = exposure;

	renderer->render_world.update_view_camera(renderer->camera_view, view, proj);
}

void renderer_add_visbuffer_hook(visbuffer_read_hook&& hook)
{
	renderer->visbuffer_read_hooks.push_back(hook);
}	

}

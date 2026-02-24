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
	vec4 cam_pos;

	GPUDevicePointer vertex_pos;
	GPUDevicePointer vertex_uv;
	GPUDevicePointer vertex_nor_tan;
	GPUDevicePointer geom_indices;
	vec2 res;
	vec2 inv_res;
	vec4 light_direction;
	vec4 light_color;
	
	float exposure;
	float ambient_factor;
	uint32_t env_brdf_handle;
	uint32_t env_irradiance_handle;
	uint32_t env_prefiltered_handle;

	float csm_cbias;
	float csm_nbias;
	float csm_scale;

	float cascade_splits[4];
	uint32_t cascade_rts[4];
	GPUDevicePointer smap_data;
};

struct Shadowmap
{
	GPUTexture texture;
	GPUTextureDescriptor descriptor;
	RenderView render_view;
	uint32_t dim;

	mat4 proj;
	mat4 view;
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
	GPUTexture hdrbuffer_tex;
	GPUTextureDescriptor visbuffer;
	GPUTextureDescriptor depthbuffer;
	GPUTextureDescriptor hdrbuffer;
	GPUTextureDescriptor hdrbuffer_rw;
	GPUTexture output_rt{0u}; 
	int tonemapper{1};

	GPUPipeline visbuffer_build_pso;
	GPUPipeline visbuffer_build_alphamask_pso;
	GPUPipeline vb_resolve_cs;
	GPUPipeline hdr_compose_pso;
	GPUPipeline brdflut_pso;

	std::vector<visbuffer_read_hook> visbuffer_read_hooks;
	
	GPUTexture brdflut_tex;
	GPUTextureDescriptor brdflut;

	RenderEnvironmentMap envmap;

	GPUPipeline shadowmap_opaque_pso;
	GPUPipeline shadowmap_alphamask_pso;

	float csm_lambda{0.9f};
	float csm_cbias{0.00125f};
	float csm_nbias{0.275f};
	float csm_scale{1.0f};
	std::vector<Shadowmap> shadowmaps;
	GPUPointer smap_data;
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

	renderer->hdrbuffer_tex = gpu_create_texture
	({
		.dim = uvec3{renderer->render_resolution, 1u},
		.format = GPU_FORMAT_B10GR11_UFLOAT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_STORAGE
	});
	renderer->hdrbuffer = gpu_texture_view_descriptor(renderer->hdrbuffer_tex, {.format = GPU_FORMAT_B10GR11_UFLOAT});
	renderer->hdrbuffer_rw = gpu_rwtexture_view_descriptor(renderer->hdrbuffer_tex, {.format = GPU_FORMAT_B10GR11_UFLOAT});

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_texture_layout_transition(cmd, renderer->visbuffer_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_texture_layout_transition(cmd, renderer->depthbuffer_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_texture_layout_transition(cmd, renderer->hdrbuffer_tex, GPU_STAGE_NONE, GPU_STAGE_COMPUTE, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
}

void renderer_generate_brdf_lut()
{
	log::info("renderer: generating BRDF lookup table: 512x512, 1024 integration steps");
	renderer->brdflut_tex = gpu_create_texture
	({
		.dim = {512u, 512u, 1u},
		.format = GPU_FORMAT_RG16_SFLOAT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_COLOR_ATTACHMENT
	});
	renderer->brdflut = gpu_texture_view_descriptor(renderer->brdflut_tex, {.format = GPU_FORMAT_RG16_SFLOAT});

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_texture_layout_transition(cmd, renderer->brdflut_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	gpu_begin_renderpass(cmd,
	{
		.color_targets = {{renderer->brdflut_tex}}
	});

	gpu_set_pipeline(cmd, renderer->brdflut_pso);
	gpu_draw(cmd, nullptr, 3, 1, 0, 0);
	gpu_end_renderpass(cmd);

	gpu_barrier(cmd, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_COMPUTE);
	gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
}

void renderer_csm_init()
{
	log::info("renderer_csm_init: {} cascades, {}x{} float16unorm shadowmap", 4, 1536, 1536);

	gpu_create_sampler
	({
		.mag_filter = GPU_FILTER_LINEAR,
		.min_filter = GPU_FILTER_LINEAR,
		.mip_filter = GPU_FILTER_LINEAR,
		.address_mode_u = GPU_ADDRESS_MODE_CLAMP_TO_BORDER,
		.address_mode_v = GPU_ADDRESS_MODE_CLAMP_TO_BORDER,
		.address_mode_w = GPU_ADDRESS_MODE_CLAMP_TO_BORDER,
		.compare_op = GPU_COMPARE_OP_LESS
	});

	renderer->shadowmaps.resize(4);
	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	for(int i = 0; i < 4; i++)
	{
		auto& smap = renderer->shadowmaps[i];

		smap.texture = gpu_create_texture
		({
			.dim = {1536u, 1536u, 1u},
			.format = GPU_FORMAT_D16_UNORM,
			.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_DEPTH_STENCIL_ATTACHMENT
		});

		smap.descriptor = gpu_texture_view_descriptor(smap.texture, {.format = GPU_FORMAT_D16_UNORM});
		smap.render_view = renderer->render_world.register_view(true);
		smap.dim = 1536u;

		gpu_texture_layout_transition(cmd, smap.texture, GPU_STAGE_NONE, GPU_STAGE_RASTER_DEPTH_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	}
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
	renderer->camera_view = renderer->render_world.register_view(false);
	
	renderer->brdflut_pso = gpu_create_graphics_pipeline(load_shader("shaders/brdflut"),
	{
		.color_targets = {GPU_FORMAT_RG16_SFLOAT}
	});

	renderer_generate_brdf_lut();

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
		vbconst->env_brdf_handle = renderer->brdflut.handle;
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
	renderer_write_material(RenderMaterialData{});

	renderer_create_rendertargets();

	renderer->visbuffer_build_pso = gpu_create_graphics_pipeline(load_shader("shaders/visbuffer_build_opaque"), 	
	{
		.color_targets = {GPU_FORMAT_R32_UINT},
		.depth_format = GPU_FORMAT_D32_SFLOAT
	});
	
	renderer->visbuffer_build_alphamask_pso = gpu_create_graphics_pipeline(load_shader("shaders/visbuffer_build_alphamask"), 	
	{
		.color_targets = {GPU_FORMAT_R32_UINT},
		.depth_format = GPU_FORMAT_D32_SFLOAT
	});

	renderer->vb_resolve_cs = gpu_create_compute_pipeline(load_shader("shaders/visbuffer_resolve"));

	renderer->hdr_compose_pso = gpu_create_graphics_pipeline(load_shader("shaders/hdr_compose"),
	{
		.color_targets = {GPU_FORMAT_RGBA8_SRGB}
	});

	renderer->shadowmap_opaque_pso = gpu_create_graphics_pipeline(load_shader("shaders/shadowmap_opaque"),
	{
		.depth_format = GPU_FORMAT_D16_UNORM
	});

	renderer->shadowmap_alphamask_pso = gpu_create_graphics_pipeline(load_shader("shaders/shadowmap_alphamask"),
	{
		.depth_format = GPU_FORMAT_D16_UNORM
	});

	renderer_csm_init();
	renderer->smap_data = gpu_allocate_memory(sizeof(mat4) * 1024, GPU_MEMORY_MAPPED);
}

void renderer_shutdown()
{
	gpu_wait_idle();

	gpu_free_memory(renderer->smap_data);

	for(auto& smap : renderer->shadowmaps)
		gpu_destroy_texture(smap.texture);

	gpu_destroy_pipeline(renderer->shadowmap_alphamask_pso);
	gpu_destroy_pipeline(renderer->shadowmap_opaque_pso);

	gpu_destroy_texture(renderer->brdflut_tex);

	gpu_destroy_pipeline(renderer->brdflut_pso);
	gpu_destroy_pipeline(renderer->hdr_compose_pso);
	gpu_destroy_pipeline(renderer->vb_resolve_cs);
	gpu_destroy_pipeline(renderer->visbuffer_build_alphamask_pso);
	gpu_destroy_pipeline(renderer->visbuffer_build_pso);

	gpu_destroy_texture(renderer->hdrbuffer_tex);
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

		//FIXME: defer old framebuffer destruction, no need to wait for queue idle
		gpu_destroy_texture(renderer->hdrbuffer_tex);
		gpu_destroy_texture(renderer->depthbuffer_tex);
		gpu_destroy_texture(renderer->visbuffer_tex);

		gpu_free_descriptor(renderer->hdrbuffer_rw);
		gpu_free_descriptor(renderer->hdrbuffer);
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
		gpu_copy_to_texture(cmd, write.data, write.texture, write.num_mips, write.num_layers);
	}
	renderer->texwrites.clear();

	renderer->transfer_resource_sync++;
	gpu_barrier(cmd, GPU_STAGE_TRANSFER, GPU_STAGE_ALL);
	gpu_emit_signal(cmd, GPU_STAGE_ALL, renderer->transfer_resource_semaphore, renderer->transfer_resource_sync);
	auto transfer_sync = gpu_submit(GPU_QUEUE_TRANSFER, cmd);
}

void renderer_build_visbuffer(GPUCommandBuffer& cmd)
{
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
	
	gpu_set_cullmode(cmd, GPU_CULLMODE_CW);

	GPUDepthStencilDesc reverse_z
	{
		.depth_mode = GPU_DEPTH_READ | GPU_DEPTH_WRITE,
		.depth_test = GPU_COMPARE_OP_GREATER
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

	auto ds_draw_data = renderer_world_get_bucket(renderer->camera_view, RENDER_BUCKET_DOUBLE_SIDED);
	gpu_set_cullmode(cmd, GPU_CULLMODE_NONE);
	
	gpu_draw_indexed_indirect_count(cmd, &shader_data, ds_draw_data.commands, ds_draw_data.counter, ds_draw_data.max_instance_count);

	gpu_set_pipeline(cmd, renderer->visbuffer_build_alphamask_pso);
	gpu_set_depth_stencil_state(cmd, reverse_z);
	
	gpu_write_cbuffer_descriptor(cmd, renderer->visbuffer_cbv[renderer->frame_index]);

	struct VBBuildAlphaData
	{
		GPUDevicePointer objects;
		GPUDevicePointer instances;
		GPUDevicePointer materials;
	} am_shader_data;
	am_shader_data.objects = shader_data.objects;
	am_shader_data.instances = shader_data.instances;
	am_shader_data.materials = gpu_host_to_device_pointer(renderer->materials.data); 

	auto am_ds_draw_data = renderer_world_get_bucket(renderer->camera_view, RENDER_BUCKET_ALPHA_MASKED_DOUBLE_SIDED);
	gpu_draw_indexed_indirect_count(cmd, &am_shader_data, am_ds_draw_data.commands, am_ds_draw_data.counter, am_ds_draw_data.max_instance_count);

	gpu_set_cullmode(cmd, GPU_CULLMODE_CW);
	auto alphamask_draw_data = renderer_world_get_bucket(renderer->camera_view, RENDER_BUCKET_ALPHA_MASKED);

	gpu_draw_indexed_indirect_count(cmd, &am_shader_data, alphamask_draw_data.commands, alphamask_draw_data.counter, alphamask_draw_data.max_instance_count);

	gpu_end_renderpass(cmd);
}

void renderer_resolve_visbuffer(GPUCommandBuffer& cmd)
{
	gpu_set_pipeline(cmd, renderer->vb_resolve_cs);

	gpu_write_cbuffer_descriptor(cmd, renderer->visbuffer_cbv[renderer->frame_index]);
	
	auto draw_data = renderer_world_get_bucket(renderer->camera_view, RENDER_BUCKET_DEFAULT);
	struct VBResolveData
	{
		GPUDevicePointer instances;
		GPUDevicePointer objects;
		GPUDevicePointer materials;
		GPUDevicePointer clusters;
		uint32_t visbuffer;
		uint32_t output;	
	} vbr_data;
	vbr_data.instances = gpu_host_to_device_pointer(draw_data.instances);
	vbr_data.objects = renderer->render_world.get_objects();
	vbr_data.materials = gpu_host_to_device_pointer(renderer->materials.data);
	vbr_data.clusters = renderer_geometry_cluster_device_pointer();
	vbr_data.visbuffer = renderer->visbuffer.handle;
	vbr_data.output = renderer->hdrbuffer_rw.handle;
	gpu_dispatch(cmd, &vbr_data, {(renderer->render_resolution.x + 7u) / 8u, (renderer->render_resolution.y + 7u) / 8u, 1u});

	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_FRAGMENT_SHADER);
}

void renderer_update_cascades(VisbufferCBuffer* vbconst, const RenderCameraData& vb_cam)
{
	std::array<vec3, 8> frustum_corners =
	{
		vec3{-1.0f, 1.0f, 0.0f},
		vec3{1.0f, 1.0f, 0.0f},
		vec3{1.0f, -1.0f, 0.0f},
		vec3{-1.0f, -1.0f, 0.0f},
		vec3{-1.0f, 1.0f, 1.0f},
		vec3{1.0f, 1.0f, 1.0f},
		vec3{1.0f, -1.0f, 1.0f},
		vec3{-1.0f, -1.0f, 1.0f}
	};

	vbconst->csm_cbias = renderer->csm_cbias;
	vbconst->csm_nbias = renderer->csm_nbias;
	vbconst->csm_scale = renderer->csm_scale / 1536.0f;

	float range = vb_cam.zfar - vb_cam.znear;
	float ratio = vb_cam.zfar / vb_cam.znear;

	std::array<float, 4> split_dist;

	for(int i = 0; i < 4; i++)
	{
		float p = (i + 1) / 4.0f;
		float log = vb_cam.znear * std::pow(ratio, p);
		float uniform = vb_cam.znear + range * p;
		float d = renderer->csm_lambda * log + (1.0f - renderer->csm_lambda) * uniform;

		split_dist[i] = (d - vb_cam.znear) / range;
	}

	/*
	 * The default projection matrix for the main camera view
	 * has its far plane set to infinity which will not work
	 * for unprojecting the frustum corners.
	 * Set a finite far plane corresponding to user settings.
	 */

	mat4 proj_finite = vb_cam.proj;
	proj_finite[2][2] = vb_cam.znear / range;
	proj_finite[3][2] = vb_cam.znear * vb_cam.zfar / range;

	mat4 inv_cam = mat4::inverse(vb_cam.view * proj_finite);
	for(int i = 0; i < 8; i++)
	{
		vec4 inv_corner = vec4{frustum_corners[i], 1.0f} * inv_cam;
		frustum_corners[i] = inv_corner.demote<3>() / inv_corner.w;
	}

	float prev_split_dist = 0.0f;
	for(int i = 0; i < 4; i++)
	{
		auto& smap = renderer->shadowmaps[i];
		vbconst->cascade_rts[i] = smap.descriptor.handle;

		std::array<vec3, 8> fc_copy;

		for(int j = 0; j < 4; j++)
		{
			auto dist = frustum_corners[j] - frustum_corners[j + 4];
			fc_copy[j] = frustum_corners[j + 4] + (dist * split_dist[i]);
			fc_copy[j + 4] = frustum_corners[j + 4] + (dist * prev_split_dist);
		}
		prev_split_dist = split_dist[i];

		vec3 fcenter{0.0f};
		for(auto c : fc_copy)
			fcenter += c;

		fcenter /= 8.0f;

		float radius = 0.0f;
		for(auto c : fc_copy)
			radius = std::max(radius, (c - fcenter).magnitude());

		radius = std::ceil(radius * 16.0f) / 16.0f;

		auto sp_point = vec4{0.0f, 0.0f, -1.0f * (vb_cam.znear + split_dist[i] * range), 1.0f} * vb_cam.proj;
		vbconst->cascade_splits[i] = sp_point.z / sp_point.w;

		// FIXME: reading light_direction over PCIe from VRAM constantbuffer
		auto pos = fcenter - (vbconst->light_direction.demote<3>() * radius * 2.0f);

		vec3 forward = vbconst->light_direction.demote<3>();
		vec3 right = vec3::normalize(vec3::cross(forward, vector_world_up));
		vec3 up = vec3::normalize(vec3::cross(right, forward));

		float tX = vec3::dot(pos, right);
		float tY = vec3::dot(pos, up);
		float tZ = vec3::dot(pos, forward);

		RenderCameraData smap_camera
		{
			.view =
			{
				vec4{right.x, up.x, -forward.x, 0.0f},
				vec4{right.y, up.y, -forward.y, 0.0f},
				vec4{right.z, up.z, -forward.z, 0.0f},
				vec4{    -tX,  -tY,       tZ, 1.0f}
			},
			.proj = mat4::make_ortho(-radius, radius, -radius, radius, 0.0f, radius * 2.0f),
			.position = pos,
			.znear = 0.0f,
			.zfar = radius * 2.0f
		};

		auto cam_mtx = smap_camera.view * smap_camera.proj;
		vec4 sorigin = vec4{0.0f, 0.0f, 0.0f, 1.0f} * cam_mtx;
		auto hres = static_cast<float>(smap.dim / 2);
		sorigin *= hres;
		vec2 rorigin = vec2{std::round(sorigin.x), std::round(sorigin.y)};
		vec2 rounding = rorigin - vec2{sorigin.x, sorigin.y};
		rounding /= hres;
		mat4 rounding_mtx = mat4::make_translation(vec3{rounding.x, rounding.y, 0.0f});

		auto mtx = cam_mtx * rounding_mtx;
		*(reinterpret_cast<mat4*>(gpu_map_memory(renderer->smap_data)) + i + (512 * renderer->frame_index)) = mtx;

		renderer->render_world.update_view_camera(smap.render_view, smap_camera);
	}
}

void renderer_update_shadowmaps(GPUCommandBuffer& cmd)
{
	GPUDepthStencilDesc shadow_ds
	{
		.depth_mode = GPU_DEPTH_READ | GPU_DEPTH_WRITE | GPU_DEPTH_CLAMP,
		.depth_test = GPU_COMPARE_OP_LESS
	};

	for(int i = 0; i < 4; i++)
	{
		auto& smap = renderer->shadowmaps[i];

		gpu_begin_renderpass(cmd,
		{
			.depth_target =
			{
				.texture = smap.texture,
				.load_op = GPU_LOAD_OP_CLEAR,
				.clear = 1.0f
			}
		});

		struct ShaderData
		{
			GPUDevicePointer objects;
			GPUDevicePointer vertex_pos;
			GPUDevicePointer smap_data;
			uint32_t smap_index;
		} shader_data;
		shader_data.objects = renderer->render_world.get_objects();
		shader_data.vertex_pos = renderer_geometry_vertex_pos_device_pointer();
		shader_data.smap_data = gpu_host_to_device_pointer(renderer->smap_data) + (512 * renderer->frame_index * sizeof(mat4));
		shader_data.smap_index = i;

		gpu_set_pipeline(cmd, renderer->shadowmap_opaque_pso);
		gpu_set_depth_stencil_state(cmd, shadow_ds);

		auto ds_draw = renderer_world_get_bucket(smap.render_view, RENDER_BUCKET_DOUBLE_SIDED);
		gpu_draw_indexed_indirect_count(cmd, &shader_data, ds_draw.commands, ds_draw.counter, ds_draw.max_instance_count);

		gpu_set_cullmode(cmd, GPU_CULLMODE_CW);

		auto draw = renderer_world_get_bucket(smap.render_view, RENDER_BUCKET_DEFAULT);
		gpu_draw_indexed_indirect_count(cmd, &shader_data, draw.commands, draw.counter, draw.max_instance_count);

		struct AMShaderData
		{
			GPUDevicePointer objects;
			GPUDevicePointer materials;
			GPUDevicePointer vertex_pos;
			GPUDevicePointer vertex_uv;
			GPUDevicePointer smap_data;
			uint32_t smap_index;
		} am_shader_data;
		am_shader_data.objects = renderer->render_world.get_objects();
		am_shader_data.materials = gpu_host_to_device_pointer(renderer->materials.data);
		am_shader_data.vertex_pos = renderer_geometry_vertex_pos_device_pointer();
		am_shader_data.vertex_uv = renderer_geometry_vertex_uv_device_pointer();
		am_shader_data.smap_data = gpu_host_to_device_pointer(renderer->smap_data) + (512 * renderer->frame_index * sizeof(mat4));
		am_shader_data.smap_index = i;

		gpu_set_pipeline(cmd, renderer->shadowmap_alphamask_pso);

		auto am_draw = renderer_world_get_bucket(smap.render_view, RENDER_BUCKET_ALPHA_MASKED);
		gpu_draw_indexed_indirect_count(cmd, &am_shader_data, am_draw.commands, am_draw.counter, am_draw.max_instance_count);

		gpu_set_cullmode(cmd, GPU_CULLMODE_NONE);

		auto ds_am_draw = renderer_world_get_bucket(smap.render_view, RENDER_BUCKET_ALPHA_MASKED_DOUBLE_SIDED);
		gpu_draw_indexed_indirect_count(cmd, &am_shader_data, ds_am_draw.commands, ds_am_draw.counter, ds_am_draw.max_instance_count);

		gpu_end_renderpass(cmd);
	}
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
	vbconst->env_irradiance_handle = renderer->envmap.irradiance.handle;
	vbconst->env_prefiltered_handle = renderer->envmap.prefiltered.handle;
	vbconst->smap_data = gpu_host_to_device_pointer(renderer->smap_data) + (512 * renderer->frame_index * sizeof(mat4));

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_wait_signal(cmd, GPU_STAGE_RASTER_COLOR_OUTPUT, renderer->swapchain_acquire[renderer->frame_index], 0);
	gpu_texture_layout_transition(cmd, renderer->cur_swapchain, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	renderer->render_world.upload_objects(cmd);
	renderer->render_world.determine_visibility(cmd);

	renderer_build_visbuffer(cmd);
	renderer_update_shadowmaps(cmd);

	gpu_barrier(cmd, GPU_STAGE_RASTER_DEPTH_OUTPUT | GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_COMPUTE);

	auto draw_data = renderer_world_get_bucket(renderer->camera_view, RENDER_BUCKET_DEFAULT);
	for(auto& hook : renderer->visbuffer_read_hooks)
		hook(cmd, {&renderer->visbuffer, gpu_host_to_device_pointer(draw_data.instances), renderer->render_world.get_objects(), renderer->render_resolution}, renderer->frame_index);

	renderer_resolve_visbuffer(cmd);

	struct HDRComposeData
	{
		uint32_t hdrbuffer_handle;
		int tonemapper;
	} compose_data;
	compose_data.hdrbuffer_handle = renderer->hdrbuffer.handle;
	compose_data.tonemapper = renderer->tonemapper;

	if(renderer->output_rt)
	{
		gpu_begin_renderpass(cmd,
		{
			.color_targets =
			{
				{
				.texture = renderer->output_rt,
				.load_op = GPU_LOAD_OP_DONTCARE
				}
			}
		});

		gpu_set_pipeline(cmd, renderer->hdr_compose_pso);

		gpu_draw(cmd, &compose_data, 3, 1, 0, 0);

		gpu_end_renderpass(cmd);

		gpu_barrier(cmd, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_FRAGMENT_SHADER);
	}

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

	if(!renderer->output_rt)
	{
		gpu_set_pipeline(cmd, renderer->hdr_compose_pso);
		gpu_draw(cmd, &compose_data, 3, 1, 0, 0);
	}

	imgui_backend_render(cmd, dt);

	gpu_end_renderpass(cmd);

	gpu_texture_layout_transition(cmd, renderer->cur_swapchain, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_ALL, GPU_TEXTURE_LAYOUT_GENERAL, GPU_TEXTURE_LAYOUT_PRESENT);
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

RenderObject renderer_world_insert_object(const RenderObjectDescription& data, uint32_t shadow_level)
{
	std::array<RenderView, 5> views
	{
		renderer->camera_view,
		renderer->shadowmaps[0].render_view,
		renderer->shadowmaps[1].render_view,
		renderer->shadowmaps[2].render_view,
		renderer->shadowmaps[3].render_view
	};

	return renderer->render_world.insert_object(data, {views.data(), shadow_level + 1u});
}

RenderBucketData renderer_world_get_bucket(RenderView view, RenderBucket bucket)
{
	return renderer->render_world.get_bucket(view, bucket);
}

void renderer_set_output_rendertarget(GPUTexture rt)
{
	renderer->output_rt = rt;
}

uvec2 renderer_get_render_resolution()
{
	return renderer->render_resolution;
}

void renderer_update_render_resolution(uvec2 res)
{
	renderer->render_resolution = res;
}

void renderer_update_camera(const RenderCameraData& cam)
{
	VisbufferCBuffer* vbconst = reinterpret_cast<VisbufferCBuffer*>(gpu_map_memory(renderer->visbuffer_cbv[renderer->frame_index]));
	vbconst->camera = cam.view * cam.proj;
	vbconst->inverse_projection = mat4::inverse(cam.proj);
	vbconst->inverse_view = mat4::inverse(cam.view);
	vbconst->cam_pos = vec4{cam.position, 1.0f};
	vbconst->exposure = cam.exposure;
	vbconst->ambient_factor = 1200.0f;

	renderer->render_world.update_view_camera(renderer->camera_view, cam);
	renderer_update_cascades(vbconst, cam);
}

void renderer_add_visbuffer_hook(visbuffer_read_hook&& hook)
{
	renderer->visbuffer_read_hooks.push_back(hook);
}	

void renderer_set_envmap(const RenderEnvironmentMap& envmap)
{
	renderer->envmap = envmap;
}

}

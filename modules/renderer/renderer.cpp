#include <penumbra/renderer.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/panic.hpp>
#include <penumbra/window.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/ui.hpp>
#include <penumbra/cvar.hpp>
#include <penumbra/config.hpp>
#include <penumbra/types.hpp>
#include <renderer/bloom.hpp>
#include <renderer/brdf.hpp>
#include <renderer/resource.hpp>
#include <renderer/shadowmap.hpp>
#include <renderer/transparent.hpp>
#include <renderer/world.hpp>
#include <renderer/visbuffer.hpp>

#include <tracy/Tracy.hpp>

#include <cassert>
#include <cmath>

namespace penumbra
{

struct renderer_context_t
{
	window_t window;

	u64 gfx_queue_frames[config::renderer_frames_in_flight];
	u64 compute_queue_frames[config::renderer_frames_in_flight];

	int frame_index;
	u32 frame_counter{0};

	GPUSemaphore swapchain_acquire[config::renderer_frames_in_flight];
	GPUSemaphore swapchain_present[config::renderer_frames_in_flight];

	GPUTexture cur_swapchain;

	uvec2 last_render_resolution{800u, 600u};
	uvec2 render_resolution{800u, 600u};
	GPUTexture output_rt{0u};
	int tonemapper{1};
	GPUPipeline hdr_compose_pso;

	render_environment_data env;

	visibility_buffer visbuffer;
	std::vector<visbuffer_hook> vb_hooks;

	GPUTexture depthbuffer_tex;
	GPUTextureDescriptor depthbuffer;

	render_shadow_data shadow_data;

	GPUTexture hdrbuffer_tex;
	GPUTextureDescriptor hdrbuffer;
	GPUTextureDescriptor hdrbuffer_rw;

	GPUTextureDescriptor brdflut;

	render_bloom_data bloom_data;
};

static renderer_context_t* renderer = nullptr;

static void set_pmode_cvar(cvar_t* cvar)
{
	gpu_swapchain_set_present_mode(GPUPresentMode(cvar->int_v));
}

static void set_wndmode_cvar(cvar_t* cvar)
{
	wm_set_fullscreen(renderer->window, cvar->int_v > 0);
}

static cvar_t vb_debug
{
	.name = "r_visbuffer_debug",
	.type = CVAR_TYPE_INT,
	.int_defv = 0,
	.int_v = 0
};

static cvar_t pr_mode
{
	.name = "r_present_mode",
	.type = CVAR_TYPE_INT,
	.int_defv = 2,
	.int_v = 2,
	.callback = set_pmode_cvar
};

static cvar_t wnd_mode
{
	.name = "r_fullscreen",
	.type = CVAR_TYPE_INT,
	.int_defv = 0,
	.int_v = 0,
	.callback = set_wndmode_cvar
};

static void renderer_init_rendertargets()
{
	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	renderer_visbuffer_init_rendertarget(renderer->visbuffer, cmd, renderer->render_resolution);

	renderer->depthbuffer_tex = gpu_create_texture
	({
		.dim = uvec3{renderer->render_resolution, 1u},
		.format = GPU_FORMAT_D32_SFLOAT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_DEPTH_STENCIL_ATTACHMENT
	});
	renderer->depthbuffer = gpu_texture_view_descriptor(renderer->depthbuffer_tex, {.format = GPU_FORMAT_D32_SFLOAT});
	gpu_texture_layout_transition(cmd, renderer->depthbuffer_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_DEPTH_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	renderer->hdrbuffer_tex = gpu_create_texture
	({
		.dim = uvec3{renderer->render_resolution, 1u},
		.format = GPU_FORMAT_B10GR11_UFLOAT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_STORAGE | GPU_TEXTURE_COLOR_ATTACHMENT
	});
	renderer->hdrbuffer = gpu_texture_view_descriptor(renderer->hdrbuffer_tex, {.format = GPU_FORMAT_B10GR11_UFLOAT});
	renderer->hdrbuffer_rw = gpu_rwtexture_view_descriptor(renderer->hdrbuffer_tex, {.format = GPU_FORMAT_B10GR11_UFLOAT});
	gpu_texture_layout_transition(cmd, renderer->hdrbuffer_tex, GPU_STAGE_NONE, GPU_STAGE_COMPUTE, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	renderer_bloom_init_rendertarget(renderer->bloom_data, cmd, renderer->render_resolution / 4);

	gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
}

static void renderer_cleanup_rendertargets()
{
	renderer_bloom_cleanup_rendertarget(renderer->bloom_data);
	
	gpu_destroy_texture(renderer->hdrbuffer_tex);
	gpu_free_descriptor(renderer->hdrbuffer);
	gpu_free_descriptor(renderer->hdrbuffer_rw);

	gpu_destroy_texture(renderer->depthbuffer_tex);
	gpu_free_descriptor(renderer->depthbuffer);

	renderer_visbuffer_cleanup_rendertarget(renderer->visbuffer);
}

static void renderer_prepare_vb_pointers()
{
	auto geometry_storage = renderer_geometry_get_storage();
	
	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		visbuffer_cbuffer* vbconst = reinterpret_cast<visbuffer_cbuffer*>(gpu_map_memory(renderer->visbuffer.cbuffer[i]));
		
		vbconst->vertex_pos = gpu_host_to_device_pointer(geometry_storage.vertex_pos);
		vbconst->vertex_uv = gpu_host_to_device_pointer(geometry_storage.vertex_uv);
		vbconst->vertex_nor_tan = gpu_host_to_device_pointer(geometry_storage.vertex_nor_tan);
		vbconst->geom_index = gpu_host_to_device_pointer(geometry_storage.index);
		vbconst->env_brdf_handle = renderer->brdflut.handle;
	}
}

static void renderer_init_samplers()
{
	gpu_create_sampler
	({
		.mag_filter = GPU_FILTER_LINEAR,
		.min_filter = GPU_FILTER_LINEAR,
		.mip_filter = GPU_FILTER_LINEAR,
		.address_mode_u = GPU_ADDRESS_MODE_CLAMP_TO_EDGE,
		.address_mode_v = GPU_ADDRESS_MODE_CLAMP_TO_EDGE,
		.address_mode_w = GPU_ADDRESS_MODE_CLAMP_TO_EDGE
	});

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
}

void renderer_init(window_t wnd)
{
	renderer = new renderer_context_t();
	renderer->window = wnd;
	
	cvar_register(&vb_debug);
	cvar_register(&pr_mode);
	cvar_register(&wnd_mode);

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

	renderer_resource_state_init();
	imgui_backend_init(renderer->window);

	renderer_world_init();
	renderer_create_view({.is_shadow = false});

	renderer_visbuffer_init(renderer->visbuffer);
	
	renderer_init_samplers();
	renderer->brdflut = renderer_brdf_init();

	renderer_shadow_init(renderer->shadow_data);
	renderer_transparent_init();
	renderer_bloom_init(renderer->bloom_data);

	renderer->hdr_compose_pso = gpu_create_graphics_pipeline(load_shader("shaders/hdr_compose"),
	{
		.color_targets = {GPU_FORMAT_BGRA8_SRGB}
	});

	renderer_prepare_vb_pointers();
	renderer_init_rendertargets();
}

void renderer_shutdown()
{
	gpu_wait_idle();
	renderer_cleanup_rendertargets();
	
	gpu_destroy_pipeline(renderer->hdr_compose_pso);

	renderer_bloom_cleanup(renderer->bloom_data);
	renderer_transparent_cleanup();
	renderer_shadow_cleanup(renderer->shadow_data);

	renderer_brdf_cleanup();

	renderer_visbuffer_cleanup(renderer->visbuffer);

	renderer_world_cleanup();

	imgui_backend_shutdown();

	renderer_resource_state_shutdown();
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
		renderer_cleanup_rendertargets();
		renderer_init_rendertargets();
	}

	renderer->frame_index = (renderer->frame_index + 1) % config::renderer_frames_in_flight;
	if(!gpu_wait_queue(GPU_QUEUE_GRAPHICS, renderer->gfx_queue_frames[renderer->frame_index]))
		panic("renderer: gfx queue stuck");

	gpu_wait_queue(GPU_QUEUE_COMPUTE, renderer->compute_queue_frames[renderer->frame_index]);
	renderer->cur_swapchain = gpu_swapchain_acquire_next(renderer->swapchain_acquire[renderer->frame_index]);
	renderer->frame_counter++;
}

static void renderer_prepare_visbuffer()
{
	vec2 f_res{static_cast<float>(renderer->render_resolution.x), static_cast<float>(renderer->render_resolution.y)};

	auto* vbconst = reinterpret_cast<visbuffer_cbuffer*>(gpu_map_memory(renderer->visbuffer.cbuffer[renderer->frame_index]));
	vbconst->res = f_res;
	vbconst->inv_res = vec2{1.0f / f_res.x, 1.0f / f_res.y};
	vbconst->light_direction = vec4{renderer->env.light_direction, 0.0f};
	vbconst->light_color = vec4{renderer->env.light_color * renderer->env.light_intensity, 0.0f};
	vbconst->ambient_factor = renderer->env.ambient_intensity;
	vbconst->env_irradiance_handle = renderer->env.envmap.irradiance.handle;
	vbconst->env_prefiltered_handle = renderer->env.envmap.prefiltered.handle;
	vbconst->csm_cbias = renderer->shadow_data.csm_cbias;
	vbconst->csm_nbias = renderer->shadow_data.csm_nbias;
	vbconst->csm_scale = renderer->shadow_data.csm_scale / 1536.0f;
	for(int i = 0; i < renderer->shadow_data.max_cascades; i++)
	{
		vbconst->cascade_rts[i] = renderer->shadow_data.cascades[i].descriptor.handle;
		vbconst->cascade_splits[i] = renderer->shadow_data.cascades[i].split_point;
	}

	vbconst->smap_data = gpu_host_to_device_pointer(renderer->shadow_data.smap_transforms) + (512 * renderer->frame_index * sizeof(mat4));
	vbconst->point_light_count = 0;
	vbconst->spot_light_count = 0;
	vbconst->cluster_sizes = vec4
	{
		std::ceilf(float(renderer->render_resolution.x) / 16.0f),
		std::ceilf(float(renderer->render_resolution.y) / 8.0f),
		0.0f, 0.0f
	};
}

static void renderer_forward_passes(GPUCommandBuffer& cmd)
{
	gpu_begin_renderpass(cmd,
	{
		.color_targets =
		{
			{
			.texture = renderer->hdrbuffer_tex,
			.load_op = GPU_LOAD_OP_LOAD
			}
		},
		.depth_target =
		{
			.texture = renderer->depthbuffer_tex,
			.load_op = GPU_LOAD_OP_LOAD
		}
	});

	renderer_transparent_draw(renderer->visbuffer, cmd);

	gpu_end_renderpass(cmd);
}

struct HDRComposeData
{
	u32 hdrbuffer_handle;
	u32 bloombuffer_handle;
	int tonemapper;
};

static void renderer_hdr_compose(GPUCommandBuffer& cmd)
{
	HDRComposeData shader_data;
	shader_data.hdrbuffer_handle = renderer->hdrbuffer.handle;
	shader_data.bloombuffer_handle = renderer->bloom_data.bloombuffer.handle;
	shader_data.tonemapper = renderer->tonemapper;

	gpu_set_pipeline(cmd, renderer->hdr_compose_pso);
	gpu_draw(cmd, &shader_data, 3, 1, 0, 0);
}

static void renderer_try_compose_output(GPUCommandBuffer& cmd)
{
	if(!renderer->output_rt)
		return;

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

	renderer_hdr_compose(cmd);
	gpu_end_renderpass(cmd);
	gpu_barrier(cmd, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_FRAGMENT_SHADER);
}

static void renderer_try_compose_swapchain(GPUCommandBuffer& cmd)
{
	if(renderer->output_rt)
		return;

	renderer_hdr_compose(cmd);
}

void renderer_process_frame(double dt)
{
	ZoneScoped;

	renderer_resource_copy_async();

	assert(renderer->cur_swapchain);

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_wait_signal(cmd, GPU_STAGE_RASTER_COLOR_OUTPUT, renderer->swapchain_acquire[renderer->frame_index], 0);
	gpu_texture_layout_transition(cmd, renderer->cur_swapchain, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	renderer_prepare_visbuffer();

	renderer_world_update(cmd);
	renderer_world_determine_visibility(cmd);

	renderer_visbuffer_build(renderer->visbuffer, cmd, renderer->depthbuffer_tex);
	renderer_shadow_build(renderer->shadow_data, cmd); 

	gpu_barrier(cmd, GPU_STAGE_RASTER_DEPTH_OUTPUT | GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_COMPUTE);
	auto drawinfo = renderer_world_get_drawcall(RENDER_VIEW_DEFAULT, RENDER_BUCKET_DEFAULT);
	visbuffer_data vbinfo
	{
		&renderer->visbuffer.descriptor,
		gpu_host_to_device_pointer(drawinfo.instances),
		gpu_host_to_device_pointer(renderer_world_get_objects()),
		renderer->render_resolution
	};
	for(auto& hook : renderer->vb_hooks)
		hook(cmd, vbinfo, renderer->frame_index);

	if(vb_debug.int_v)
		renderer_visbuffer_visualize(renderer->visbuffer, cmd, renderer->hdrbuffer_rw);
	else
		renderer_visbuffer_resolve(renderer->visbuffer, cmd, renderer->hdrbuffer_rw);

	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_RASTER_COLOR_OUTPUT);

	renderer_forward_passes(cmd);

	gpu_barrier(cmd, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_COMPUTE);

	renderer_bloom_process(renderer->bloom_data, cmd, renderer->hdrbuffer);

	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_FRAGMENT_SHADER);

	renderer_try_compose_output(cmd);

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

	renderer_try_compose_swapchain(cmd);
	imgui_backend_render(cmd, dt);

	gpu_end_renderpass(cmd);

	gpu_texture_layout_transition(cmd, renderer->cur_swapchain, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_ALL, GPU_TEXTURE_LAYOUT_GENERAL, GPU_TEXTURE_LAYOUT_PRESENT);
	gpu_emit_signal(cmd, GPU_STAGE_ALL, renderer->swapchain_present[renderer->frame_index], 0);
	auto gfx_sync = gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	renderer->gfx_queue_frames[renderer->frame_index] = gfx_sync;

	gpu_swapchain_present(GPU_QUEUE_GRAPHICS, renderer->swapchain_present[renderer->frame_index]);
}

u32 renderer_gfx_frame_index()
{
	return renderer->frame_index;
}

uvec2 renderer_get_render_resolution()
{
	return renderer->render_resolution;
}

void renderer_update_render_resolution(uvec2 res)
{
	renderer->render_resolution = res;
}

void renderer_set_output_rendertarget(GPUTexture rt)
{
	renderer->output_rt = rt;
}

void renderer_update_camera(const render_camera_data& data)
{
	auto* vbconst = reinterpret_cast<visbuffer_cbuffer*>(gpu_map_memory(renderer->visbuffer.cbuffer[renderer->frame_index]));
	vbconst->camera = data.view * data.proj;
	vbconst->view = data.view;
	vbconst->inv_proj = mat4::inverse(data.proj);
	vbconst->inv_view = mat4::inverse(data.view);
	vbconst->cam_pos = vec4{data.position, 1.0f};
	vbconst->exposure = data.exposure;
	vbconst->cluster_scale = 24.0f / std::log2f(data.zfar / data.znear);
	vbconst->cluster_bias = -(24.0f * std::log2f(data.znear) / std::log2f(data.zfar / data.znear));
	vbconst->znear = data.znear;
	vbconst->zfar = data.zfar;

	renderer_update_view(RENDER_VIEW_DEFAULT, data);
	renderer_shadow_update(renderer->shadow_data, data, renderer->env.light_direction); 
}

void renderer_update_environment(const render_environment_data& data)
{
	renderer->env = data;
}

renderObjectID renderer_world_insert_object(const render_object_desc& desc, u32 shadow_level)
{
	std::array<renderViewID, 4> views
	{
		RENDER_VIEW_DEFAULT,
		renderer->shadow_data.cascades[0].render_view,
		renderer->shadow_data.cascades[1].render_view,
		renderer->shadow_data.cascades[2].render_view,
	};

	return renderer_world_insert_object_internal(desc, {views.data(), shadow_level + 1u});
}

void renderer_hook_visbuffer(const visbuffer_hook& hook)
{
	renderer->vb_hooks.push_back(hook);
}

}

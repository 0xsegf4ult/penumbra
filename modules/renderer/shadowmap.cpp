#include <renderer/shadowmap.hpp>
#include <renderer/resource.hpp>
#include <renderer/world.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/math/matrix.hpp>
#include <penumbra/log.hpp>
#include <penumbra/types.hpp>
#include <penumbra/cvar.hpp>
#include <vector>

namespace penumbra
{

static GPUPipeline shadowmap_opaque_pso;
static GPUPipeline shadowmap_alphamask_pso;
static GPUDepthStencilDesc shadow_ds;

constexpr u32 CSM_DIM = 1536u;
constexpr u32 CSM_CASCADES = 3;

static cvar_t cvar_max_cascades
{
	.name = "r_csm_cascades",
	.type = CVAR_TYPE_INT,
	.int_defv = CSM_CASCADES,
	.int_v = CSM_CASCADES
};

static cvar_t cvar_csm_lambda
{
	.name = "r_csm_lambda",
	.type = CVAR_TYPE_FLOAT,
	.float_defv = 0.9f,
	.float_v = 0.9f
};

static cvar_t cvar_cbias
{
	.name = "r_csm_cbias",
	.type = CVAR_TYPE_FLOAT,
	.float_defv = 0.00125f,
	.float_v = 0.00125f
};

static cvar_t cvar_nbias
{
	.name = "r_csm_nbias",
	.type = CVAR_TYPE_FLOAT,
	.float_defv = 0.275f,
	.float_v = 0.275f
};

void renderer_shadow_init(render_shadow_data& data)
{
	shadowmap_opaque_pso = gpu_create_graphics_pipeline(load_shader("shaders/shadowmap_opaque"),
	{
		.depth_format = GPU_FORMAT_D16_UNORM
	});

	shadowmap_alphamask_pso = gpu_create_graphics_pipeline(load_shader("shaders/shadowmap_alphamask"),
	{
		.depth_format = GPU_FORMAT_D16_UNORM
	});

	shadow_ds = GPUDepthStencilDesc
	{
		.depth_mode = GPU_DEPTH_READ | GPU_DEPTH_WRITE | GPU_DEPTH_CLAMP,
		.depth_test = GPU_COMPARE_OP_LESS
	};
	
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

	cvar_register(&cvar_max_cascades);
	cvar_register(&cvar_csm_lambda);
	cvar_register(&cvar_cbias);
	cvar_register(&cvar_nbias);

	data.smap_transforms = gpu_allocate_memory(sizeof(mat4) * 512 * 2, GPU_MEMORY_MAPPED);

	data.max_cascades = CSM_CASCADES; 
	data.cascades.resize(data.max_cascades);
	
	log::info("renderer_csm_init: {} cascades, {}x{} unorm16 shadowmap", data.max_cascades, CSM_DIM, CSM_DIM);	

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	for(int i = 0; i < data.max_cascades; i++)
	{
		auto& cascade = data.cascades[i];
		cascade.dim = CSM_DIM;

		cascade.texture = gpu_create_texture
		({
			.dim = {cascade.dim, cascade.dim, 1u},
			.format = GPU_FORMAT_D16_UNORM,
			.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_DEPTH_STENCIL_ATTACHMENT
		});

		cascade.descriptor = gpu_texture_view_descriptor(cascade.texture, {.format = GPU_FORMAT_D16_UNORM});
		cascade.render_view = renderer_create_view({.is_shadow = true});

		gpu_texture_layout_transition(cmd, cascade.texture, GPU_STAGE_NONE, GPU_STAGE_RASTER_DEPTH_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	}
	gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
}

void renderer_shadow_cleanup(render_shadow_data& data)
{
	for(auto& cascade : data.cascades)
	{
		gpu_destroy_texture(cascade.texture);
		gpu_free_descriptor(cascade.descriptor);
	}

	gpu_free_memory(data.smap_transforms);
	
	gpu_destroy_pipeline(shadowmap_alphamask_pso);
	gpu_destroy_pipeline(shadowmap_opaque_pso);
}

void renderer_shadow_update(render_shadow_data& data, const render_camera_data& main_camera, vec3 light_dir)
{
	data.csm_lambda = std::max(0.0f, cvar_csm_lambda.float_v);
	data.csm_cbias = std::max(0.0f, cvar_cbias.float_v);
	data.csm_nbias = std::max(0.0f, cvar_nbias.float_v);
	data.max_cascades = std::min(u32(std::max(0, cvar_max_cascades.int_v)), CSM_CASCADES);
	u32 maxc = data.max_cascades;

	vec3 frustum_corners[8] =
	{
		vec3{-1.0f, 1.0f, 0.0f},
		vec3{1.0f, 1.0f, 0.0f},
		vec3{1.0f, -1.0f, 0.0f},
		vec3{-1.0f, -1.0f, 0.0f},
		vec3{-1.0f, 1.0f, 1.0f},
		vec3{1.0f, 1.0f, 1.0f},
		vec3{1.0f, -1.0f, 1.0f},
		vec3{-1.0f, -1.0f, -1.0f}
	};

	float range = main_camera.zfar - main_camera.znear;
       	float ratio = main_camera.zfar / main_camera.znear;

	float split_dist[4];

	for(u32 i = 0; i < maxc; i++)
	{	
		float p = (i + 1) / static_cast<float>(maxc);
		float log = main_camera.znear * std::pow(ratio, p);
		float uniform = main_camera.znear + range * p;
		float d = data.csm_lambda * log + (1.0f - data.csm_lambda) * uniform;

		split_dist[i] = (d - main_camera.znear) / range;
	}

	/*
         * The default projection matrix for the main camera view
         * has its far plane set to infinity which will not work
         * for unprojecting the frustum corners.
         * Set a finite far plane corresponding to user settings.
         */
	mat4 proj_finite = main_camera.proj;
	proj_finite[2][2] = main_camera.znear / range;
	proj_finite[3][2] = main_camera.znear * main_camera.zfar / range;

	mat4 inv_cam = mat4::inverse(main_camera.view * proj_finite);
	for(int i = 0; i < 8; i++)
	{
		vec4 inv_corner = vec4{frustum_corners[i], 1.0f} * inv_cam;
		frustum_corners[i] = inv_corner.demote<3>() / inv_corner.w;
	}
		
	vec3 forward = light_dir;
	vec3 right = vec3::normalize(vec3::cross(forward, vector_world_up));
	vec3 up = vec3::normalize(vec3::cross(right, forward));

	float prev_split_dist = 0.0f;
	for(u32 i = 0; i < maxc; i++)
	{
		auto& cascade = data.cascades[i];

		vec3 fc_copy[8];
		for(int j = 0; j < 4; j++)
		{
			auto dist = frustum_corners[j] - frustum_corners[j + 4];
			fc_copy[j] = frustum_corners[j + 4] + (dist * split_dist[i]);
			fc_copy[j + 4] = frustum_corners[j + 4] + (dist * prev_split_dist);
		}
		prev_split_dist = split_dist[i];

		vec3 fcenter{0.0f};
		for(int i = 0; i < 8; i++)
			fcenter += fc_copy[i];

		fcenter /= 8.0f;

		float radius = 0.0f;
		for(int i = 0; i < 8; i++)
			radius = std::max(radius, (fc_copy[i] - fcenter).magnitude());

		radius = std::ceil(radius * 16.0f) / 16.0f;

		auto sp_point = vec4{0.0f, 0.0f, -1.0f * (main_camera.znear + split_dist[i] * range), 1.0f} * main_camera.proj;
		cascade.split_point = sp_point.z / sp_point.w;

	 	auto pos = fcenter - (light_dir * radius * 2.0f);

		float tX = vec3::dot(pos, right);
		float tY = vec3::dot(pos, up);
		float tZ = vec3::dot(pos, forward);

		render_camera_data smap_camera
		{
			.view = 
			{
				vec4{right.x, up.x, -forward.x, 0.0f},
				vec4{right.y, up.y, -forward.y, 0.0f},
				vec4{right.z, up.z, -forward.z, 0.0f},
				vec4{    -tX,  -tY,         tZ, 1.0f}
			},
			.proj = mat4::make_ortho(-radius, radius, -radius, radius, 0.0f, radius * 2.0f),
			.position = pos,
			.znear = 0.0f,
			.zfar = radius * 2.0f
		};

		auto cam_mtx = smap_camera.view * smap_camera.proj;
		vec4 sorigin = vec4{0.0f, 0.0f, 0.0f, 1.0f} * cam_mtx;
		auto hres = static_cast<float>(cascade.dim / 2);
		sorigin *= hres;
		vec2 rorigin = vec2{std::round(sorigin.x), std::round(sorigin.y)};
		vec2 rounding = rorigin - vec2{sorigin.x, sorigin.y};
		rounding /= hres;
		mat4 rounding_mtx = mat4::make_translation(vec3{rounding.x, rounding.y, 0.0f});

		auto mtx = cam_mtx * rounding_mtx;
		memcpy(gpu_map_memory(data.smap_transforms) + (i + 512 * renderer_gfx_frame_index()) * sizeof(mat4), &mtx, sizeof(mat4));
		renderer_update_view(cascade.render_view, smap_camera);
	}
}

void renderer_shadow_build(render_shadow_data& data, GPUCommandBuffer& cmd)
{
	auto geometry_storage = renderer_geometry_get_storage();

	struct ShaderData
	{
		GPUDevicePointer objects;
		GPUDevicePointer vertex_pos;
		GPUDevicePointer smap_data;
		u32 smap_index;
	} shader_data;
	shader_data.objects = gpu_host_to_device_pointer(renderer_world_get_objects());
	shader_data.vertex_pos = gpu_host_to_device_pointer(geometry_storage.vertex_pos);
	shader_data.smap_data = gpu_host_to_device_pointer(data.smap_transforms) + (512 * renderer_gfx_frame_index() * sizeof(mat4));

	struct AMShaderData
	{
		GPUDevicePointer objects;
		GPUDevicePointer materials;
		GPUDevicePointer vertex_pos;
		GPUDevicePointer vertex_uv;
		GPUDevicePointer smap_data;
		u32 smap_index;
	} am_shader_data;
	am_shader_data.objects = shader_data.objects;
	am_shader_data.materials = gpu_host_to_device_pointer(renderer_material_get_storage());
	am_shader_data.vertex_pos = shader_data.vertex_pos;
	am_shader_data.vertex_uv = gpu_host_to_device_pointer(geometry_storage.vertex_uv);
	am_shader_data.smap_data = shader_data.smap_data;

	gpu_bind_index_buffer(cmd, geometry_storage.index, GPU_INDEX_TYPE_U8);
	
	for(u32 i = 0; i < data.max_cascades; i++)
	{
		auto& cascade = data.cascades[i];

		gpu_begin_renderpass(cmd,
		{
			.depth_target =
			{
				.texture = cascade.texture,
				.load_op = GPU_LOAD_OP_CLEAR,
				.clear = 1.0f
			}
		});

		shader_data.smap_index = i;

		gpu_set_pipeline(cmd, shadowmap_opaque_pso);
		gpu_set_depth_stencil_state(cmd, shadow_ds);
		
		auto drawcall = renderer_world_get_drawcall(cascade.render_view, RENDER_BUCKET_DOUBLE_SIDED);
		gpu_draw_indexed_indirect_count(cmd, &shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);

		gpu_set_cullmode(cmd, GPU_CULLMODE_CW);
		drawcall = renderer_world_get_drawcall(cascade.render_view, RENDER_BUCKET_DEFAULT);
		gpu_draw_indexed_indirect_count(cmd, &shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);

		am_shader_data.smap_index = i;
		gpu_set_pipeline(cmd, shadowmap_alphamask_pso);

		drawcall = renderer_world_get_drawcall(cascade.render_view, RENDER_BUCKET_ALPHA_MASKED);
		gpu_draw_indexed_indirect_count(cmd, &am_shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);

		gpu_set_cullmode(cmd, GPU_CULLMODE_NONE);
		drawcall = renderer_world_get_drawcall(cascade.render_view, RENDER_BUCKET_ALPHA_MASKED_DOUBLE_SIDED);
		gpu_draw_indexed_indirect_count(cmd, &am_shader_data, drawcall.commands, drawcall.counter, drawcall.max_instance_count);

		gpu_end_renderpass(cmd);
	}
}

}

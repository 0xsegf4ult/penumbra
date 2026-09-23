#pragma once

#include <penumbra/array_proxy.hpp>
#include <penumbra/math/matrix.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/resource/rid.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/window.hpp>
#include <penumbra/types.hpp>

#include <functional>

namespace penumbra
{

enum render_gpu_pass : u32
{
	RENDER_GPU_PASS_FRAME,
	RENDER_GPU_PASS_UPDATE,
	RENDER_GPU_PASS_CULL,
	RENDER_GPU_PASS_PREPASS,
	RENDER_GPU_PASS_SHADOW,
	RENDER_GPU_PASS_RESOLVE,
	RENDER_GPU_PASS_FORWARD,
	RENDER_GPU_PASS_BLOOM,
	RENDER_GPU_PASS_COMPOSE,
	RENDER_GPU_PASS_COUNT
};

enum render_bucket
{
	RENDER_BUCKET_DEFAULT,
	RENDER_BUCKET_DOUBLE_SIDED,
	RENDER_BUCKET_ALPHA_MASKED,
	RENDER_BUCKET_ALPHA_MASKED_DOUBLE_SIDED,
	RENDER_BUCKET_TRANSPARENT,
	RENDER_BUCKET_TRANSPARENT_DOUBLE_SIDED,
	RENDER_BUCKET_COUNT
};

enum render_light_type : u32
{
	RENDER_LIGHT_TYPE_POINT		= 0,
	RENDER_LIGHT_TYPE_SPOT 		= 1,
};

using renderViewID = u32;
using renderObjectID = u32;
using renderLightID = u32;

constexpr u32 RENDER_GPU_PASS_BANK_SIZE = RENDER_GPU_PASS_COUNT * 2;
constexpr renderViewID RENDER_VIEW_DEFAULT{1};

struct render_gpu_timings
{
	u32 frame{0u};
	u64 ns[RENDER_GPU_PASS_COUNT][2]{};
};

struct render_camera_data
{
	mat4 view;
	mat4 proj;
	vec3 position;
	float znear;
	float zfar;
	float exposure;
};

struct render_environment_map
{
	GPUTextureDescriptor irradiance;
	GPUTextureDescriptor prefiltered;
};

struct render_environment_data
{
	vec3 light_direction{0.0f, -1.0f, 0.0f};
	vec3 light_color{1.0f};
	float light_intensity{1.0f};
	float ambient_intensity{1.0f};
	render_environment_map envmap;
};

struct render_view_desc
{
	bool is_shadow{false};
};

struct render_object_desc
{
	mat4 transform;
	ResourceID geometry;
	ResourceID material{0u};
	ResourceID skeleton{0u};
};

struct render_light_desc
{
	render_light_type type{RENDER_LIGHT_TYPE_POINT};
	vec3 position{0.0f};
	vec3 direction{0.0f, 0.0f, -1.0f};
	vec3 color{1.0f};
	float intensity{1.0f};
	float radius{5.0f};
	float inner_cone{42.5f};
	float outer_cone{45.0f};
	bool shadowcast{true};
};

struct visbuffer_data
{
	GPUTextureDescriptor* texture;
	GPUDevicePointer instances;
	GPUDevicePointer objects;
	uvec2 resolution;
};

using visbuffer_hook = std::function<void(GPUCommandBuffer&, visbuffer_data, u32)>;

void renderer_init(window_t wnd);
void renderer_shutdown();

void renderer_next_frame();
void renderer_process_frame(double dt);
u32 renderer_gfx_frame_index();
const render_gpu_timings& renderer_gpu_timings();

uvec2 renderer_get_render_resolution();
void renderer_update_render_resolution(uvec2 res);
void renderer_set_output_rendertarget(GPUTexture rt);

void renderer_update_camera(const render_camera_data& data);
void renderer_update_environment(const render_environment_data& data);

renderViewID renderer_create_view(const render_view_desc& desc);
void renderer_update_view(renderViewID view, const render_camera_data& camera);
renderObjectID renderer_world_insert_object(const render_object_desc& desc);
void renderer_world_remove_object(renderObjectID object);
void renderer_world_set_object_visible(renderObjectID object, bool visible);
void renderer_world_update_object(renderObjectID object, const mat4& transform);
void renderer_world_update_skin(renderObjectID object, const mat4* bones, u16 count);

renderLightID renderer_world_insert_light(const render_light_desc& desc);
void renderer_world_update_light(renderLightID light, const render_light_desc& desc);
void renderer_world_set_light_visible(renderLightID light, bool visible);
void renderer_world_remove_light(renderLightID light);

void renderer_hook_visbuffer(const visbuffer_hook& hook);

}

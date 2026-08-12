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

using renderViewID = u32;
using renderObjectID = u32;

constexpr renderViewID RENDER_VIEW_DEFAULT{1};

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
	vec3 light_direction;
	vec3 light_color;
	float light_intensity;
	float ambient_intensity;
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

uvec2 renderer_get_render_resolution();
void renderer_update_render_resolution(uvec2 res);
void renderer_set_output_rendertarget(GPUTexture rt);

void renderer_update_camera(const render_camera_data& data);
void renderer_update_environment(const render_environment_data& data);

renderViewID renderer_create_view(const render_view_desc& desc);
void renderer_update_view(renderViewID view, const render_camera_data& camera);
renderObjectID renderer_world_insert_object(const render_object_desc& desc, u32 shadow_level = 3);
void renderer_world_update_object(renderObjectID object, const mat4& transform);
void renderer_world_update_skin(renderObjectID object, const mat4* bones, u16 count);

void renderer_hook_visbuffer(const visbuffer_hook& hook);

}

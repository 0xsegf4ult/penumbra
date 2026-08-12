#pragma once

#include <penumbra/gpu.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/config.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct visbuffer_cbuffer
{
	mat4 camera;
	mat4 view;
	mat4 inv_proj;
	mat4 inv_view;
	vec4 cam_pos;

	GPUDevicePointer vertex_pos;
	GPUDevicePointer vertex_uv;
	GPUDevicePointer vertex_nor_tan;
	GPUDevicePointer geom_index;
	vec2 res;
	vec2 inv_res;
	vec4 light_direction;
	vec4 light_color;

	float exposure;
	float ambient_factor;
	u32 env_brdf_handle;
	u32 env_irradiance_handle;
	u32 env_prefiltered_handle;

	float csm_cbias;
	float csm_nbias;
	float csm_scale;

	float cascade_splits[4];
	u32 cascade_rts[4];
	GPUDevicePointer smap_data;

 	GPUDevicePointer point_lights;
        GPUDevicePointer spot_lights;
        GPUDevicePointer light_clusters;
        GPUDevicePointer light_counters;
        GPUDevicePointer point_light_indices;
        GPUDevicePointer spot_light_indices;
        GPUDevicePointer point_light_grid;
        GPUDevicePointer spot_light_grid;

	u32 point_light_count;
	u32 spot_light_count;
	float cluster_scale;
	float cluster_bias;
	vec4 cluster_sizes;

	float znear;
	float zfar;
};

struct visibility_buffer
{
	GPUTexture texture;
	GPUTextureDescriptor descriptor;

	uvec2 resolution;
	GPUPointer cbuffer[config::renderer_frames_in_flight];
};

void renderer_visbuffer_init(visibility_buffer& visbuffer);
void renderer_visbuffer_cleanup(visibility_buffer& visbuffer);

void renderer_visbuffer_init_rendertarget(visibility_buffer& visbuffer, GPUCommandBuffer& cmd, uvec2 resolution);
void renderer_visbuffer_cleanup_rendertarget(visibility_buffer& visbuffer);

void renderer_visbuffer_build(visibility_buffer& visbuffer, GPUCommandBuffer& cmd, GPUTexture zbuffer);
void renderer_visbuffer_visualize(visibility_buffer& visbuffer, GPUCommandBuffer& cmd, GPUTextureDescriptor& output);
void renderer_visbuffer_resolve(visibility_buffer& visbuffer, GPUCommandBuffer& cmd, GPUTextureDescriptor& hdr_output);

}

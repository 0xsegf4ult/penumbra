#pragma once

#include "globals.slang"

static const uint LIGHT_CLUSTERS_X = 16;
static const uint LIGHT_CLUSTERS_Y = 8;
static const uint LIGHT_CLUSTERS_Z = 4;
static const uint LIGHT_CLUSTERS_Z_ALL = 24;

struct Light
{
	float4 pos_ic;
	float4 color_rad;
	float4 dir_oc;
};

struct LightCluster
{
	float4 min;
	float4 max;
};

struct LightGrid
{
	uint offset;
	uint count;
};

struct VisbufferConstants
{
	float4x4 camera;
	float4x4 view;
	float4x4 inverse_projection;
	float4x4 inverse_view;
	float4 cam_pos;
	
	Ptr<float3, Access.Read> vertex_pos;
	Ptr<float2, Access.Read> vertex_uv;
	Ptr<uint2, Access.Read> vertex_nor_tan;
	Ptr<uint8_t, Access.Read> ibuf;
	float2 res;
	float2 inv_res;
	float4 light_direction;
	float4 light_color;

	float exposure;
	float ambient_factor;
	ResourceHandle<Texture2D<float2>> brdf;
	ResourceHandle<TextureCube<float3>> irradiance;
	ResourceHandle<TextureCube<float3>> prefiltered;
	
	float csm_cbias;
	float csm_nbias;
	float csm_scale;
	
	float cascade_splits[4];
	ResourceHandle<Texture2D> cascade_rts[4];
	Ptr<float4x4, Access.Read> smap_data;
	Ptr<Light, Access.Read> point_lights;
	Ptr<Light, Access.Read> spot_lights;
	Ptr<LightCluster, Access.ReadWrite> light_clusters;
	Ptr<uint32_t, Access.ReadWrite> light_counters;
	Ptr<uint32_t, Access.ReadWrite> point_light_indices;
	Ptr<uint32_t, Access.ReadWrite> spot_light_indices;
	Ptr<LightGrid, Access.ReadWrite> point_light_grid;
	Ptr<LightGrid, Access.ReadWrite> spot_light_grid;

	uint32_t point_light_count;
	uint32_t spot_light_count;
	float cluster_scale;
	float cluster_bias;
	float4 cluster_sizes;

	float znear;
	float zfar;
};

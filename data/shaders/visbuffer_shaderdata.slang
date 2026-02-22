#pragma once

#include "globals.slang"

struct VisbufferConstants
{
	float4x4 camera;
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
};

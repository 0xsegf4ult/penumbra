#pragma once

#include <penumbra/resource/rid.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/types.hpp>
#include <string>

namespace penumbra
{

enum material_flags_t : u32
{
	MATERIAL_ALPHA_MASK	= 0x1,
	MATERIAL_ALPHA_BLEND	= 0x2,
	MATERIAL_DOUBLE_SIDED	= 0x4,
	MATERIAL_CLEARCOAT	= 0x8,
	MATERIAL_ANISOTROPIC	= 0x10
};

struct material_factors
{
	vec4 albedo{1.0f};
	float roughness{1.0f};
	float metallic{1.0f};
	float normal{1.0f};
	float reflectivity{0.5f};
	float alpha_cf{0.5f};
	vec3 emissive{0.0f};
};

struct clearcoat_info
{
	float factor{1.0f};
	float roughness_factor{1.0f};
};

struct material_resource 
{
	std::string name;

	material_factors factors;
	u32 flags;

	ResourceID albedo;
	ResourceID mro;
	ResourceID normalmap;
	ResourceID emissive;

	clearcoat_info clearcoat;
};

}

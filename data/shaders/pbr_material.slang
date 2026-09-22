#pragma once

#include "globals.slang"

[Flags]
enum MaterialFlags : uint
{
	AlphaMask,
	AlphaBlend,
	DoubleSided,
	Clearcoat,
	Anisotropy
};	

struct MaterialPBR
{
	float4 albedo_factor;
	float roughness_factor;
	float metallic_factor;
	float normal_factor;
	float reflectivity;
	float alpha_cf;
	float3 emissive_factor;
	uint flags;

	ResourceHandle<Texture2D> albedo;
	ResourceHandle<Texture2D> mro;
	ResourceHandle<Texture2D> normalmap;
	ResourceHandle<Texture2D> emissive;

	float clearcoat_factor;
	float clearcoat_roughness_factor;
};

float DistributionGGX(float NdotH, float roughness)
{
	float a = roughness * NdotH;
	float k = a / (1.0 - NdotH * NdotH + a * a);
	return k * k / float.getPi();
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	
	float denom = NdotV * (1.0 - k) + k;
	return NdotV / denom;
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float3 fresnelSchlick(float HdotV, float3 f0)
{
	return f0 + (1.0 - f0) * pow5(1.0 - HdotV);
}

float3 F_Schlick(float3 f0, float f90, float u)
{
	return f0 + (f90 - f0) * pow5(1.0 - u);
}

float V_SmithGGXCorrelated(float NdotL, float NdotV, float alphaG)
{
	float alphaG2 = alphaG * alphaG;
	float ggxV = NdotL * sqrt((-NdotV * alphaG2 + NdotV) * NdotV + alphaG2);
	float ggxL = NdotV * sqrt((-NdotL * alphaG2 + NdotL) * NdotL + alphaG2);
	return 0.5 / (ggxV + ggxL);
}

float D_GGX(float NdotH, float m)
{
	float m2 = m * m;
	float f = (NdotH * m2 - NdotH) * NdotH + 1.0;
	return m2 / (f * f);
}

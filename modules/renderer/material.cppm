export module penumbra.renderer:material;

import penumbra.math;
import penumbra.gpu;
import std;
using std::uint32_t;

namespace penumbra
{

export enum RenderMaterialFlags : uint32_t
{
	RENDER_MATERIAL_ALPHA_MASK = 0x1,
	RENDER_MATERIAL_ALPHA_BLEND = 0x2,
	RENDER_MATERIAL_DOUBLESIDED = 0x4
};

export struct RenderMaterialFactors
{
	vec3 diffuse{1.0f};
	float roughness{1.0f};
	float metallic{1.0f};
	float normal{1.0f};
	float reflectivity{0.5f};
	float alpha_cf{0.5f};
	vec3 emissive{0.0f};
};

export struct RenderMaterialData
{
	RenderMaterialFactors factors{};
	uint32_t flags{0u};

	uint32_t albedo{0u};
	uint32_t mro{0u};
	uint32_t normalmap{0u};
	uint32_t emissive{0u};
};

export struct RenderMaterialStorage
{
	GPUPointer data;
	uint32_t size{0u};
	uint32_t capacity{65536u};
};

}

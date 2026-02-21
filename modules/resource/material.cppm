export module penumbra.resource:material;

import :resource_id;

import penumbra.math;
import penumbra.renderer;
import std;
using std::uint32_t;

namespace penumbra
{

export struct MaterialResource
{
	std::string name;

	RenderMaterialFactors factors;
	uint32_t flags;

	ResourceID albedo;
	ResourceID mro;
	ResourceID normalmap;
	ResourceID emissive;
};

}

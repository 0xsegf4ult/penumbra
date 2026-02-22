export module penumbra.editor:envmap;

import penumbra.core;
import penumbra.renderer;
import penumbra.resource;
import std;
using std::uint32_t;

namespace penumbra
{

export struct EnvironmentMap
{
	ResourceID irradiance;
	ResourceID prefiltered;
};

export EnvironmentMap load_envmap(const vfs::path& path)
{
	EnvironmentMap res{};

	res.irradiance = resource_manager_load_texture(path / "env_irradiance");
	res.prefiltered = resource_manager_load_texture(path / "env_prefiltered");

	return res;
}

}

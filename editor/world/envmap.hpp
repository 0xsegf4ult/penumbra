#pragma once

#include <penumbra/resource.hpp>
#include <penumbra/vfs.hpp>

namespace penumbra
{

struct EnvironmentMap
{
	ResourceID irradiance;
	ResourceID prefiltered;
};

inline EnvironmentMap load_envmap(const vfs_path& path)
{
	EnvironmentMap res{};

	res.irradiance = resource_manager_load_texture(path / "env_irradiance");
	res.prefiltered = resource_manager_load_texture(path / "env_prefiltered");

	return res;
}

}

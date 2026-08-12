#pragma once

#include <penumbra/resource/rid.hpp>
#include <penumbra/vfs.hpp>
#include <penumbra/types.hpp>

#include <map>
#include <vector>

namespace penumbra
{

struct WorldState;

struct CachedPrimitive
{
	ResourceID geometry;
	ResourceID material;
};

struct gltf_import_context
{
	WorldState* world;
	std::map<size_t, std::vector<CachedPrimitive>> mesh_map;
	std::map<size_t, ResourceID> texture_map;
	std::map<size_t, ResourceID> material_map;
	std::map<size_t, ResourceID> skeleton_map;
	std::map<size_t, u16> node_to_bone_map;
};

bool import_gltf(gltf_import_context& ctx, const vfs_path& path);

}

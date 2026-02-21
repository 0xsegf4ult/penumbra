export module penumbra.resource:api;

import :resource_id;
import :geometry;
import :material;
import :texture;

import penumbra.core;

import std;

using std::uint8_t, std::uint32_t;

namespace penumbra
{

export void resource_manager_init();
export void resource_manager_shutdown();

export ResourceID resource_manager_load_geometry(const vfs::path& path);
export ResourceID resource_manager_load_texture(const vfs::path& path);
export ResourceID resource_manager_create_material(MaterialResource&& data);

export const GeometryResource& resource_manager_get_geometry(const ResourceID& rid);
export const TextureResource& resource_manager_get_texture(const ResourceID& rid);
export const MaterialResource& resource_manager_get_material(const ResourceID& rid);

}

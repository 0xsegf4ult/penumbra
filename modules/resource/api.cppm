export module penumbra.resource:api;

import :resource_id;
import :geometry;
import :material;
import :texture;

import penumbra.core;
import penumbra.gpu;

import std;

using std::uint8_t, std::uint32_t;

namespace penumbra
{

export void resource_manager_init();
export void resource_manager_shutdown();

export ResourceID resource_manager_import_geometry(const GeometryResource& res);
export ResourceID resource_manager_import_texture(std::string_view name, const GPUTextureDesc& info, std::span<const std::byte> data);

export ResourceID resource_manager_load_geometry(const vfs::path& path);
export ResourceID resource_manager_load_texture(const vfs::path& path);
export ResourceID resource_manager_create_material(MaterialResource&& data);

export GeometryResource& resource_manager_get_geometry(const ResourceID& rid);
export TextureResource& resource_manager_get_texture(const ResourceID& rid);
export MaterialResource& resource_manager_get_material(const ResourceID& rid);

export std::span<GeometryResource> resource_manager_get_geometry_storage();
export std::span<TextureResource> resource_manager_get_texture_storage();
export std::span<MaterialResource> resource_manager_get_material_storage();

export void resource_manager_sync_material(const ResourceID& rid);

}

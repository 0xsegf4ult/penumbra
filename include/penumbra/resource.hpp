#pragma once

#include <penumbra/resource/rid.hpp>
#include <penumbra/resource/anim.hpp>
#include <penumbra/resource/geometry.hpp>
#include <penumbra/resource/texture.hpp>
#include <penumbra/resource/material.hpp>
#include <penumbra/vfs.hpp>
#include <penumbra/types.hpp>

#include <string>
#include <span>

namespace penumbra
{

struct GPUTextureDesc;	

void resource_manager_init();
void resource_manager_shutdown();

ResourceID resource_manager_import_geometry(const geometry_import_desc& desc);
ResourceID resource_manager_import_texture(std::string_view name, const GPUTextureDesc& info, std::span<const u8> data);
ResourceID resource_manager_import_material(const material_resource& data);
ResourceID resource_manager_import_animation(const animation_resource& data);
ResourceID resource_manager_import_skeleton(const skeleton_resource& data);

ResourceID resource_manager_load_geometry(const vfs_path& path);
ResourceID resource_manager_load_texture(const vfs_path& path);
ResourceID resource_manager_load_animation(const vfs_path& path);
ResourceID resource_manager_load_skeleton(const vfs_path& path);

geometry_resource& resource_manager_get_geometry(ResourceID rid);
texture_resource& resource_manager_get_texture(ResourceID rid);
material_resource& resource_manager_get_material(ResourceID rid);
animation_resource& resource_manager_get_animation(ResourceID rid);
skeleton_resource& resource_manager_get_skeleton(ResourceID rid);

void resource_manager_sync_material(ResourceID rid);

}

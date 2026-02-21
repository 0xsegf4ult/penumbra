module;

#include <cassert>

module penumbra.resource;

import :resource_id;
import :geometry;
import :texture;
import :material;

import penumbra.core;
import penumbra.gpu;
import penumbra.renderer;

import std;

using std::uint8_t, std::uint32_t;

namespace penumbra
{

struct resource_context
{
	std::vector<GeometryResource> geometry;
	std::vector<MaterialResource> material;
	std::vector<TextureResource> texture;
	std::unordered_map<uint32_t, ResourceID> geometry_cache;
	std::unordered_map<uint32_t, ResourceID> texture_cache;
};

resource_context* context = nullptr;

void resource_manager_init()
{
	context = new resource_context();
}

void resource_manager_shutdown()
{
	for(auto& tex : context->texture)
		gpu_destroy_texture(tex.texture);
	
	delete context;
}

ResourceID resource_manager_load_geometry(const vfs::path& path)
{
	auto phash = fnv::hash(path.c_str());
	if(context->geometry_cache.contains(phash))
		return context->geometry_cache[phash];

	auto file = vfs::open(path, vfs::access_readonly);
	if(!file.has_value())
	{
		log::error("resource_manager: loading geometry [{}] failed: {}", path.string(), vfs::file_open_error_to_string(file.error()));
		return ResourceID{};
	}	

	const auto* data = vfs::map<std::byte>(*file, vfs::access_readonly);
	const auto* header = reinterpret_cast<const GeometryFileFormat::Header*>(data);
       	if(header->magic != GeometryFileFormat::fmt_magic || header->vmajor != GeometryFileFormat::fmt_major_version)
	{
		log::error("resource_manager: loading geometry [{}] failed: invalid file", path.string());
		return ResourceID{};
	}

	uint32_t vcount = 0;
	uint32_t icount = 0;
	uint32_t ccount = 0;

	const auto* lod_table = reinterpret_cast<const GeometryFileFormat::LOD*>(data + header->lod_offset);
	const auto* cluster_table = reinterpret_cast<const GeometryFileFormat::Cluster*>(data + header->cluster_offset);

	for(uint32_t l = 0; l < header->num_lods; l++)
	{
		ccount += lod_table[l].cluster_count;
		for(uint32_t i = 0; i < lod_table[l].cluster_count; i++)
		{
			uint32_t coff = i + lod_table[l].cluster_offset;
			vcount += cluster_table[coff].vertex_count;
			icount += cluster_table[coff].index_count;
		}
	}

	std::vector<geom_cluster_format> clusters(ccount);
	std::vector<geom_lod_format> lods(header->num_lods);
	memcpy(clusters.data(), data + header->cluster_offset, sizeof(geom_cluster_format) * ccount);
	memcpy(lods.data(), data + header->lod_offset, sizeof(geom_lod_format) * header->num_lods);

	auto voff = renderer_geometry_push_vertices
	(
		reinterpret_cast<const geom_position_format*>(data + header->vpos_offset), 
		reinterpret_cast<const geom_uv_format*>(data + header->vuv_offset), 
		reinterpret_cast<const geom_nor_tan_format*>(data + header->vnorms_offset), 
		vcount
	);
	auto ioff = renderer_geometry_push_indices(reinterpret_cast<const geom_index_format*>(data + header->index_offset), icount);

	for(auto& cluster : clusters)
	{
		cluster.vertex_offset += voff;
		cluster.index_offset += ioff;
	}

	auto coff = renderer_geometry_push_clusters(clusters.data(), ccount);
	
	for(auto& lod : lods)
		lod.cluster_offset += coff;
	
	auto loff = renderer_geometry_push_lods(lods.data(), header->num_lods);

	context->geometry.push_back
	({
		voff,
		vcount,
		ioff,
		icount,
		coff,
		ccount,
		lods[0].cluster_count,
		loff, 
		header->num_lods,
		renderer_resource_transfer_syncval() + 1
	});	
	
	auto rid = ResourceID{ResourceType::Geometry, static_cast<uint32_t>(context->geometry.size())};
	context->geometry_cache[phash] = rid;
	return rid;
}

ResourceID resource_manager_load_texture(const vfs::path& path)
{
	auto phash = fnv::hash(path.c_str());
	if(context->texture_cache.contains(phash))
		return context->texture_cache[phash];

	
	auto file = vfs::open(path, vfs::access_readonly);
	if(!file.has_value())
	{
		log::error("resource_manager: loading texture [{}] failed: {}", path.string(), vfs::file_open_error_to_string(file.error()));
		return ResourceID{};
	}

	const auto* ptr = vfs::map<std::byte>(*file, vfs::access_readonly);
	const auto* header = reinterpret_cast<const TextureFileFormat::Header*>(ptr);
	if(header->magic != TextureFileFormat::fmt_magic || header->vmajor != TextureFileFormat::fmt_major_version || header->texformat == TextureFileFormat::TextureFormat::Invalid)
	{
		log::error("resource_manager: loading texture [{}] failed: invalid file", path.string());
		return ResourceID{};
	}

	const auto* res_table = reinterpret_cast<const TextureFileFormat::SubresourceDescription*>(ptr + header->subres_desc_offset);
	uint32_t tex_size = 0u;
	uint32_t num_mips = 0;
	uint32_t num_layers = 0;
	for(uint32_t l = 0; l < header->num_subres; l++)
	{
		tex_size += res_table[l].data_size_bytes;
		num_mips = std::max(num_mips, res_table[l].level + 1);
		num_layers = std::max(num_layers, res_table[l].layer + 1);
	}

	GPUTexture tex = gpu_create_texture
	({
		.dim = {res_table[0].width, res_table[0].height, 1u},
		.mip_count = num_mips,
		.layer_count = num_layers,
		.format = TextureFileFormat::parse_format(header->texformat),
		.usage = GPU_TEXTURE_SAMPLED
	});	 

	renderer_write_texture(tex, {ptr + res_table[0].data_offset, tex_size}, num_mips, num_layers);

	GPUTextureDescriptor descriptor = gpu_texture_view_descriptor(tex, {.format = TextureFileFormat::parse_format(header->texformat)});

	context->texture.push_back
	({
		path.string(),
		tex,
		descriptor,
		renderer_resource_transfer_syncval() + 1
	});

	auto rid = ResourceID{ResourceType::Texture, static_cast<uint32_t>(context->texture.size())};
	context->texture_cache[phash] = rid;
	return rid;
}

ResourceID resource_manager_create_material(MaterialResource&& data)
{
	context->material.push_back(data);
	renderer_write_material
	({
	 	.factors = data.factors,
		.flags = data.flags,
		.albedo = data.albedo.get_handle() ? resource_manager_get_texture(data.albedo).descriptor.handle : 0u,
		.mro = data.mro.get_handle() ? resource_manager_get_texture(data.mro).descriptor.handle : 0u,
		.normalmap = data.normalmap.get_handle() ? resource_manager_get_texture(data.normalmap).descriptor.handle : 0u,
		.emissive = data.emissive.get_handle() ? resource_manager_get_texture(data.emissive).descriptor.handle : 0u,
	});
	auto rid = ResourceID{ResourceType::Material, static_cast<uint32_t>(context->material.size())};
	return rid;
}

const GeometryResource& resource_manager_get_geometry(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Geometry);
	assert(rid.get_handle());
	return context->geometry[rid.get_handle() - 1];
}

const TextureResource& resource_manager_get_texture(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Texture);
	assert(rid.get_handle());
	return context->texture[rid.get_handle() - 1];
}

const MaterialResource& resource_manager_get_material(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Material);
	assert(rid.get_handle());
	return context->material[rid.get_handle() - 1];
}

}

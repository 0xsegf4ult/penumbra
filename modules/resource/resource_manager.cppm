module;

#include <cassert>

export module penumbra.resource;

import :geometry;

import penumbra.core;
import penumbra.gpu;
import penumbra.renderer;

import std;

using std::uint8_t, std::uint32_t;

namespace penumbra
{

enum class ResourceType : uint8_t
{
	Invalid,
	Geometry,
	Texture,
	Material,
};

export struct ResourceID
{
public:
	constexpr ResourceID() : internal{0u} {}
	constexpr ResourceID(ResourceType type, uint32_t handle) : internal{std::to_underlying(type) << 24 | handle & 0xFFFFFF} {}

	constexpr ResourceType get_type() const
	{
		return ResourceType{static_cast<uint8_t>(internal >> 24)};
	}

	constexpr uint32_t get_handle() const
	{
		return internal & 0xFFFFFF;
	}
private:
	uint32_t internal;
};

struct resource_context
{
	std::vector<GeometryResource> geometry;
	std::unordered_map<uint32_t, ResourceID> geometry_cache;
};

resource_context* context = nullptr;

export void resource_manager_init()
{
	context = new resource_context();
}

export void resource_manager_shutdown()
{
	delete context;
}

export ResourceID resource_manager_load_geometry(const vfs::path& path)
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

export const GeometryResource& resource_manager_get_geometry(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Geometry);
	return context->geometry[rid.get_handle() - 1];
}

}

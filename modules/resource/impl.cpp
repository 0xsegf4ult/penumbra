module;

#include <cassert>

module penumbra.resource;

import :resource_id;
import :geometry;
import :texture;
import :material;
import :animation;
import :skeleton;

import penumbra.core;
import penumbra.gpu;
import penumbra.renderer;
import penumbra.anim;

import std;

using std::uint8_t, std::uint16_t, std::uint32_t, std::size_t;

namespace penumbra
{

struct resource_context
{
	std::vector<GeometryResource> geometry;
	std::vector<MaterialResource> material;
	std::vector<TextureResource> texture;
	std::vector<Animation> animation;
	std::vector<Skeleton> skeleton;

	std::unordered_map<uint32_t, ResourceID> geometry_cache;
	std::unordered_map<uint32_t, ResourceID> texture_cache;
	std::unordered_map<uint32_t, ResourceID> animation_cache;
	std::unordered_map<uint32_t, ResourceID> skeleton_cache;
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

ResourceID resource_manager_import_geometry(const GeometryResource& res)
{
	context->geometry.emplace_back(std::move(res));
	auto rid = ResourceID{ResourceType::Geometry, static_cast<uint32_t>(context->geometry.size())};
	return rid;
}

ResourceID resource_manager_import_texture(std::string_view name, const GPUTextureDesc& info, std::span<const std::byte> data)
{
	GPUTexture tex = gpu_create_texture(info);

	renderer_write_texture(tex, data, info.mip_count, info.layer_count);

	GPUTextureDescriptor descriptor = gpu_texture_view_descriptor(tex, {.type = info.type, .format = info.format}); 

	context->texture.push_back
	({
		std::string{name},
		tex,
		descriptor,
		renderer_resource_transfer_syncval() + 1
	});

	auto rid = ResourceID{ResourceType::Texture, static_cast<uint32_t>(context->texture.size())};
	return rid;
}

ResourceID resource_manager_import_animation(const Animation& anim)
{
	context->animation.emplace_back(std::move(anim));
	auto rid = ResourceID{ResourceType::Animation, static_cast<uint32_t>(context->animation.size())};
	return rid;
}

ResourceID resource_manager_import_skeleton(const Skeleton& skel)
{
	context->skeleton.emplace_back(std::move(skel));
	auto rid = ResourceID{ResourceType::Skeleton, static_cast<uint32_t>(context->skeleton.size())};
	return rid;
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

	bool is_skinned = (header->vert_format == GeometryFileFormat::VertexFormat::Skinned); 

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

	uint32_t voff;

	if(is_skinned)
	{
		voff = renderer_write_skinned_vertices(reinterpret_cast<const geom_skinned_format*>(data + header->vpos_offset), vcount); 
	}
	else
	{
		voff = renderer_geometry_push_vertices
		(
			reinterpret_cast<const geom_position_format*>(data + header->vpos_offset), 
			reinterpret_cast<const geom_uv_format*>(data + header->vuv_offset), 
			reinterpret_cast<const geom_nor_tan_format*>(data + header->vnorms_offset), 
			vcount
		);
	}
	auto ioff = renderer_geometry_push_indices(reinterpret_cast<const geom_index_format*>(data + header->index_offset), icount);
	auto coff = renderer_geometry_push_clusters(clusters.data(), ccount);
	auto loff = renderer_geometry_push_lods(lods.data(), header->num_lods);

	context->geometry.push_back
	({
		path.filename().string(),
 		voff,
		vcount,
		ioff,
		icount,
		coff,
		ccount,
		lods[0].cluster_count,
		loff, 
		header->num_lods,
		header->sphere,
		renderer_resource_transfer_syncval() + 1,
		is_skinned
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
	 	.type = num_layers == 6 ? GPU_TEXTURE_CUBE : GPU_TEXTURE_2D,
		.dim = {res_table[0].width, res_table[0].height, 1u},
		.mip_count = num_mips,
		.layer_count = num_layers,
		.format = TextureFileFormat::parse_format(header->texformat),
		.usage = GPU_TEXTURE_SAMPLED
	});	 

	renderer_write_texture(tex, {ptr + res_table[0].data_offset, tex_size}, num_mips, num_layers);

	GPUTextureDescriptor descriptor = gpu_texture_view_descriptor(tex, {.type = num_layers == 6 ? GPU_TEXTURE_CUBE : GPU_TEXTURE_2D, .format = TextureFileFormat::parse_format(header->texformat)});

	context->texture.push_back
	({
		path.filename().string(),
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
		.clearcoat = data.clearcoat
	});
	auto rid = ResourceID{ResourceType::Material, static_cast<uint32_t>(context->material.size())};
	return rid;
}

ResourceID resource_manager_load_animation(const vfs::path& path)
{
	auto phash = fnv::hash(path.c_str());
	if(context->animation_cache.contains(phash))
		return context->animation_cache[phash];

	auto file = vfs::open(path, vfs::access_readonly);
	if(!file.has_value())
	{
		log::error("resource_manager: loading animation [{}] failed: {}", path.string(), vfs::file_open_error_to_string(file.error()));
		return ResourceID{};
	}	

	const auto* pdata = vfs::map<std::byte>(*file, vfs::access_readonly);
	const auto* header = reinterpret_cast<const AnimationFileFormat::Header*>(pdata);

	if(header->magic != AnimationFileFormat::fmt_magic || header->vmajor != AnimationFileFormat::fmt_major_version)
	{
		log::error("resource_manager: loading animation [{}] failed: invalid file", path.string());
		return ResourceID{};

	}

	Animation anim;
	anim.name = path.filename().string();
	anim.channels.resize(header->channel_count);

	const auto* chan_table = reinterpret_cast<const AnimationFileFormat::Channel*>(pdata + header->channel_table_offset);
	for(uint32_t i = 0; i < header->channel_count; i++)
	{
		AnimationChannel& chn = anim.channels[i];
		chn.timestamps.resize(chan_table[i].keyframe_count);
		chn.bone = chan_table[i].bone - 1u;
		chn.path = static_cast<AnimationPath>(chan_table[i].path);
		chn.interp = static_cast<AnimationInterp>(chan_table[i].interp);

		std::memcpy(chn.timestamps.data(), pdata + chan_table[i].timestamp_offset, sizeof(float) * chan_table[i].keyframe_count);

		anim.start_time = std::min(anim.start_time, chn.timestamps[0]);
		anim.end_time = std::max(anim.end_time, chn.timestamps.back());

		auto esize_for_path = [](AnimationPath ap) -> size_t
		{
			switch(ap)
			{
			using enum AnimationPath;
			case Translation:
			case Scale:
				return 3zu;
			case Rotation:
				return 4zu;
			default:
				std::unreachable();
			}
		};

		chn.values.resize(chan_table[i].keyframe_count * esize_for_path(chn.path));
		std::memcpy(chn.values.data(), pdata + chan_table[i].value_offset, esize_for_path(chn.path) * chan_table[i].keyframe_count * sizeof(float));
	}

	context->animation.emplace_back(std::move(anim));

	auto rid = ResourceID{ResourceType::Animation, static_cast<uint32_t>(context->animation.size())};
	context->animation_cache[phash] = rid;
	return rid;
}

ResourceID resource_manager_load_skeleton(const vfs::path& path)
{
	auto phash = fnv::hash(path.c_str());
        if(context->skeleton_cache.contains(phash))
                return context->skeleton_cache[phash];

        auto file = vfs::open(path, vfs::access_readonly);
        if(!file.has_value())
        {
                log::error("resource_manager: loading skeleton [{}] failed: {}", path.string(), vfs::file_open_error_to_string(file.error()));
                return ResourceID{};
        }

        const auto* pdata = vfs::map<std::byte>(*file, vfs::access_readonly);
        const auto* header = reinterpret_cast<const SkeletonFileFormat::Header*>(pdata);

        if(header->magic != SkeletonFileFormat::fmt_magic || header->vmajor != SkeletonFileFormat::fmt_major_version)
        {
                log::error("resource_manager: loading skeleton [{}] failed: invalid file", path.string());
               return ResourceID{};

        }

	Skeleton skel;
	skel.name = path.filename().string();
	skel.bone_count = static_cast<uint16_t>(header->bone_count);
	skel.bone_names.resize(header->bone_count);
	skel.bone_transforms.resize(header->bone_count);
	skel.bone_parents.resize(header->bone_count);
	skel.bone_inv_bind_matrices.resize(header->bone_count);

	const auto* string_table = reinterpret_cast<const char*>(pdata + header->name_table_offset);
	const auto* transform_table = reinterpret_cast<const Transform*>(pdata + header->transform_table_offset);
	const auto* parent_table = reinterpret_cast<const uint32_t*>(pdata + header->parent_table_offset);
	const auto* matrix_table = reinterpret_cast<const mat4*>(pdata + header->matrix_table_offset);

	for(uint32_t i = 0; i < header->bone_count; i++)
	{
		skel.bone_names[i] = std::string(string_table);
		string_table += skel.bone_names[i].length() + 1;

		skel.bone_transforms[i] = transform_table[i];
		skel.bone_parents[i] = static_cast<uint16_t>(parent_table[i]);
		skel.bone_inv_bind_matrices[i] = matrix_table[i];
	}

	context->skeleton.emplace_back(std::move(skel));
	auto rid = ResourceID{ResourceType::Skeleton, static_cast<uint32_t>(context->skeleton.size())};
	context->skeleton_cache[phash] = rid;
	return rid;
}

GeometryResource& resource_manager_get_geometry(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Geometry);
	assert(rid.get_handle());
	return context->geometry[rid.get_handle() - 1];
}

TextureResource& resource_manager_get_texture(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Texture);
	assert(rid.get_handle());
	return context->texture[rid.get_handle() - 1];
}

MaterialResource& resource_manager_get_material(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Material);
	assert(rid.get_handle());
	return context->material[rid.get_handle() - 1];
}

Animation& resource_manager_get_animation(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Animation);
	assert(rid.get_handle());
	return context->animation[rid.get_handle() - 1];
}

Skeleton& resource_manager_get_skeleton(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Skeleton);
	assert(rid.get_handle());
	return context->skeleton[rid.get_handle() - 1];
}

std::span<GeometryResource> resource_manager_get_geometry_storage()
{
	return context->geometry;
}

std::span<TextureResource> resource_manager_get_texture_storage()
{
	return context->texture;
}

std::span<MaterialResource> resource_manager_get_material_storage()
{
	return context->material;
}

std::span<Animation> resource_manager_get_animation_storage()
{
	return context->animation;
}

std::span<Skeleton> resource_manager_get_skeleton_storage()
{
	return context->skeleton;
}

void resource_manager_sync_material(const ResourceID& rid)
{
	assert(rid.get_type() == ResourceType::Material);
	assert(rid.get_handle());
	auto& res = context->material[rid.get_handle() - 1];

	renderer_write_material(rid.get_handle(),
	{
		.factors = res.factors,
		.flags = res.flags,
		.albedo = res.albedo.get_handle() ? resource_manager_get_texture(res.albedo).descriptor.handle : 0u,
		.mro = res.mro.get_handle() ? resource_manager_get_texture(res.mro).descriptor.handle : 0u,
		.normalmap = res.normalmap.get_handle() ? resource_manager_get_texture(res.normalmap).descriptor.handle : 0u,
		.emissive = res.emissive.get_handle() ? resource_manager_get_texture(res.emissive).descriptor.handle : 0u,
		.clearcoat = res.clearcoat
	});
}

}

#include <penumbra/resource.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/hash.hpp>
#include <penumbra/log.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/types.hpp>
#include <penumbra/vfs.hpp>
#include <renderer/resource.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

using std::memcpy;

namespace penumbra
{

struct resource_context
{
	std::vector<geometry_resource> geometry;
	std::vector<texture_resource> texture;
	std::vector<material_resource> material;
	std::vector<animation_resource> animation;
	std::vector<skeleton_resource> skeleton;

	std::unordered_map<u32, ResourceID> geometry_cache;
	std::unordered_map<u32, ResourceID> texture_cache;
	std::unordered_map<u32, ResourceID> animation_cache;
	std::unordered_map<u32, ResourceID> skeleton_cache;
};

static resource_context* context = nullptr;

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

ResourceID resource_manager_import_geometry(const geometry_import_desc& desc)
{
	bool is_skinned = desc.vertex_skinned != nullptr;
	assert((desc.vertex_pos && desc.vertex_uv && desc.vertex_nor_tan) || desc.vertex_skinned);
	assert(desc.index);
	assert(desc.cluster);
	assert(desc.lod);
	assert(desc.vertex_count);
	assert(desc.index_count);
	assert(desc.cluster_count);
	assert(desc.lod_count);

	u32 voff;
	if(is_skinned)
	{
		voff = renderer_geometry_write_skinned(desc.vertex_skinned, desc.vertex_count);
	}
	else
	{
		voff = renderer_geometry_write_vertices
		(
			desc.vertex_pos,
			desc.vertex_uv,
			desc.vertex_nor_tan,
			desc.vertex_count
		);
	}

	u32 ioff = renderer_geometry_write_indices(desc.index, desc.index_count);
	u32 coff = renderer_geometry_write_clusters(desc.cluster, desc.cluster_count);
	u32 loff = renderer_geometry_write_lods(desc.lod, desc.lod_count);

	context->geometry.push_back
	({
		desc.name,
		voff,
		desc.vertex_count,
		ioff,
		desc.index_count,
		coff,
		desc.cluster_count,
		desc.lod[0].cluster_count,
		loff,
		desc.lod_count,
		desc.sphere,
		renderer_resource_transfer_syncval() + 1,
		is_skinned
	});

	return resource_id_new(RESOURCE_TYPE_GEOMETRY, static_cast<u32>(context->geometry.size()));
}

ResourceID resource_manager_import_texture(std::string_view name, const GPUTextureDesc& info, std::span<const u8> data)
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

	return resource_id_new(RESOURCE_TYPE_TEXTURE, static_cast<u32>(context->texture.size()));
}

ResourceID resource_manager_import_material(const material_resource& data)
{
	context->material.push_back(data);
	renderer_write_material(static_cast<u32>(context->material.size()),
	{
		.factors = data.factors,
		.flags = data.flags,
		.albedo = resource_get_handle(data.albedo) ? resource_manager_get_texture(data.albedo).descriptor.handle : 0u,
		.mro = resource_get_handle(data.mro) ? resource_manager_get_texture(data.mro).descriptor.handle : 0u,
		.normalmap = resource_get_handle(data.normalmap) ? resource_manager_get_texture(data.normalmap).descriptor.handle : 0u,
		.emissive = resource_get_handle(data.emissive) ? resource_manager_get_texture(data.emissive).descriptor.handle : 0u,
		.clearcoat = data.clearcoat
	});
	return resource_id_new(RESOURCE_TYPE_MATERIAL, static_cast<u32>(context->material.size()));
}

ResourceID resource_manager_import_animation(const animation_resource& data)
{
	context->animation.push_back(data);
	return resource_id_new(RESOURCE_TYPE_ANIMATION, static_cast<u32>(context->animation.size()));
}

ResourceID resource_manager_import_skeleton(const skeleton_resource& data)
{
	context->skeleton.push_back(data);
	return resource_id_new(RESOURCE_TYPE_SKELETON, static_cast<u32>(context->skeleton.size()));
}

ResourceID resource_manager_load_geometry(const vfs_path& path)
{
	auto phash = fnv::hash(path.c_str());
	if(context->geometry_cache.contains(phash))
		return context->geometry_cache[phash];

	auto file = vfs_open(path, VFS_ACCESS_READ);
	if(file < 0)
	{
		log::error("resource_manager: loading geometry [{}] failed: could not open file", path.string());
		return ResourceID{0};
	}

	const auto* data = vfs_map(file);
	const auto* header = reinterpret_cast<const GeometryFileFormat::Header*>(data);
	if(header->magic != GeometryFileFormat::fmt_magic || header->vmajor != GeometryFileFormat::fmt_major)
	{
		log::error("resource_manager: loading geometry [{}] failed: invalid file", path.string());
		vfs_close(file);
		return ResourceID{0};
	}

	bool is_skinned = (header->vert_format == VERTEX_FORMAT_SKINNED);

	u32 vcount = 0;
	u32 icount = 0;
	u32 ccount = 0;

	const auto* lod_table = reinterpret_cast<const GeometryFileFormat::LOD*>(data + header->lod_offset);
	const auto* cluster_table = reinterpret_cast<const GeometryFileFormat::Cluster*>(data + header->cluster_offset);

	for(u32 l = 0; l < header->num_lods; l++)
	{
		ccount += lod_table[l].cluster_count;
		for(u32 i = 0; i < lod_table[l].cluster_count; i++)
		{
			u32 coff = i + lod_table[l].cluster_offset;
			vcount += cluster_table[coff].vertex_count;
			icount += cluster_table[coff].index_count;
		}
	}

	geometry_import_desc import_desc{};
	import_desc.name = path.filename().string();
	import_desc.vertex_count = vcount;
	import_desc.index_count = icount;
	import_desc.cluster_count = ccount;
	import_desc.lod_count = header->num_lods;
	import_desc.sphere = header->sphere; 

	if(is_skinned)
	{
		import_desc.vertex_skinned = reinterpret_cast<const geom_skinned_format*>(data + header->vpos_offset);
	}
	else
	{
		import_desc.vertex_pos = reinterpret_cast<const geom_position_format*>(data + header->vpos_offset);
		import_desc.vertex_uv = reinterpret_cast<const geom_uv_format*>(data + header->vuv_offset);
		import_desc.vertex_nor_tan = reinterpret_cast<const geom_nor_tan_format*>(data + header->vnorms_offset);
	}

	import_desc.index = reinterpret_cast<const geom_index_format*>(data + header->index_offset);
	import_desc.cluster = reinterpret_cast<const geom_cluster_format*>(data + header->cluster_offset);
	import_desc.lod = reinterpret_cast<const geom_lod_format*>(data + header->lod_offset);

	auto rid = resource_manager_import_geometry(import_desc);
	context->geometry_cache[phash] = rid;
	vfs_close(file);
	return rid;
}

ResourceID resource_manager_load_texture(const vfs_path& path)
{
	auto phash = fnv::hash(path.c_str());
	if(context->texture_cache.contains(phash))
		return context->texture_cache[phash];

	auto file = vfs_open(path, VFS_ACCESS_READ);
	if(file < 0)
	{
		log::error("resource_manager: loading texture [{}] failed: could not open file", path.string());
		return ResourceID{0};
	}

	const auto* ptr = vfs_map(file);
	const auto* header = reinterpret_cast<const TextureFileFormat::Header*>(ptr);
	if(header->magic != TextureFileFormat::fmt_magic || header->vmajor != TextureFileFormat::fmt_major)
	{
		log::error("resource_manager: loading texture [{}] failed: invalid file", path.string());
		vfs_close(file);
		return ResourceID{0};
	}

	const auto* res_table = reinterpret_cast<const TextureFileFormat::SubresourceDescription*>(ptr + header->subres_desc_offset);
	u32 tex_size = 0u;
	u32 num_mips = 0u;
	u32 num_layers = 0u;
	for(u32 l = 0; l < header->num_subres; l++)
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

	auto rid = resource_id_new(RESOURCE_TYPE_TEXTURE, static_cast<u32>(context->texture.size()));
	context->texture_cache[phash] = rid;
	vfs_close(file);
	return rid;
}

ResourceID resource_manager_load_animation(const vfs_path& path)
{
	auto phash = fnv::hash(path.c_str());
	if(context->animation_cache.contains(phash))
		return context->animation_cache[phash];

	auto file = vfs_open(path, VFS_ACCESS_READ);
	if(file < 0)
	{
		log::error("resource_manager: loading animation [{}] failed: could not open file", path.string());
		return ResourceID{};
	}

	const auto* pdata = vfs_map(file);
	const auto* header = reinterpret_cast<const AnimationFileFormat::Header*>(pdata);

	if(header->magic != AnimationFileFormat::fmt_magic || header->vmajor != AnimationFileFormat::fmt_major)
	{
		log::error("resource_manager: loading animation [{}] failed: invalid file", path.string());
		vfs_close(file);
		return ResourceID{};
	}

	animation_resource anim;
	anim.name = path.filename().string();
	anim.channels.resize(header->channel_count);
	anim.start_time = std::numeric_limits<float>::max();
	anim.end_time = 0.0f;
		

	const auto* chan_table = reinterpret_cast<const AnimationFileFormat::Channel*>(pdata + header->channel_table_offset);
	for(u32 i = 0; i < header->channel_count; i++)
	{
		anim_channel& chn = anim.channels[i];
		chn.timestamps.resize(chan_table[i].keyframe_count);
		chn.bone = chan_table[i].bone - 1u;
		chn.path = static_cast<animation_path_t>(chan_table[i].path);
		chn.interp = static_cast<animation_interp_t>(chan_table[i].interp);

		memcpy(chn.timestamps.data(), pdata + chan_table[i].timestamp_offset, sizeof(float) * chan_table[i].keyframe_count);

		anim.start_time = std::min(anim.start_time, chn.timestamps[0]);
		anim.end_time = std::max(anim.end_time, chn.timestamps.back());

		auto esize_for_path = [](animation_path_t ap) -> size_t
		{
			switch(ap)
			{
			case ANIM_PATH_TRANSLATION:
			case ANIM_PATH_SCALE:
				return 3zu;
			case ANIM_PATH_ROTATION:
				return 4zu;
			default:
				std::unreachable();
			}
		};

		chn.values.resize(chan_table[i].keyframe_count * esize_for_path(chn.path));
		memcpy(chn.values.data(), pdata + chan_table[i].value_offset, esize_for_path(chn.path) * chan_table[i].keyframe_count * sizeof(float));
	}

	auto rid = resource_manager_import_animation(anim);
	context->animation_cache[phash] = rid;
	vfs_close(file);
	return rid;
}

ResourceID resource_manager_load_skeleton(const vfs_path& path)
{
	auto phash = fnv::hash(path.c_str());
	if(context->skeleton_cache.contains(phash))
		return context->skeleton_cache[phash];

	auto file = vfs_open(path, VFS_ACCESS_READ);
	if(file < 0)
	{
		log::error("resource_manager: loading skeleton [{}] failed: could not open file", path.string());
		return ResourceID{};
	}

	const auto* pdata = vfs_map(file);
	const auto* header = reinterpret_cast<const SkeletonFileFormat::Header*>(pdata);

	if(header->magic != SkeletonFileFormat::fmt_magic || header->vmajor != SkeletonFileFormat::fmt_major)
	{
		log::error("resource_manager: loading skeleton [{}] failed: invalid file", path.string());
		vfs_close(file);
		return ResourceID{};
	}

	skeleton_resource skel;
	skel.name = path.filename().string();
	skel.bone_count = static_cast<u16>(header->bone_count);
	skel.bone_names.resize(header->bone_count);
	skel.bone_transforms.resize(header->bone_count);
	skel.bone_parents.resize(header->bone_count);
	skel.bone_inv_bind_matrices.resize(header->bone_count);

	const auto* string_table = reinterpret_cast<const char*>(pdata + header->name_table_offset);
	const auto* transform_table = reinterpret_cast<const Transform*>(pdata + header->transform_table_offset);
	const auto* parent_table = reinterpret_cast<const u32*>(pdata + header->parent_table_offset);
	const auto* matrix_table = reinterpret_cast<const mat4*>(pdata + header->matrix_table_offset);

	for(u32 i = 0; i < header->bone_count; i++)
	{
		skel.bone_names[i] = std::string(string_table);
		string_table += skel.bone_names[i].length() + 1;

		skel.bone_transforms[i] = transform_table[i];
		skel.bone_parents[i] = static_cast<u16>(parent_table[i]);
		skel.bone_inv_bind_matrices[i] = matrix_table[i];
	}

	auto rid = resource_manager_import_skeleton(skel);
	context->skeleton_cache[phash] = rid;
	vfs_close(file);
	return rid;
}

geometry_resource& resource_manager_get_geometry(ResourceID rid)
{
	assert(resource_get_type(rid) == RESOURCE_TYPE_GEOMETRY);
	assert(resource_get_handle(rid));
	return context->geometry[resource_get_handle(rid) - 1];
}

texture_resource& resource_manager_get_texture(ResourceID rid)
{
	assert(resource_get_type(rid) == RESOURCE_TYPE_TEXTURE);
	assert(resource_get_handle(rid));
	return context->texture[resource_get_handle(rid) - 1];
}

material_resource& resource_manager_get_material(ResourceID rid)
{
	assert(resource_get_type(rid) == RESOURCE_TYPE_MATERIAL);
	assert(resource_get_handle(rid));
	return context->material[resource_get_handle(rid) - 1];
}

animation_resource& resource_manager_get_animation(ResourceID rid)
{
	assert(resource_get_type(rid) == RESOURCE_TYPE_ANIMATION);
	assert(resource_get_handle(rid));
	return context->animation[resource_get_handle(rid) - 1];
}

skeleton_resource& resource_manager_get_skeleton(ResourceID rid)
{
	assert(resource_get_type(rid) == RESOURCE_TYPE_SKELETON);
	assert(resource_get_handle(rid));
	return context->skeleton[resource_get_handle(rid) - 1];
}

void resource_manager_sync_material(ResourceID rid)
{
	auto& res = resource_manager_get_material(rid);

	renderer_write_material(resource_get_handle(rid),
	{
		.factors = res.factors,
                .flags = res.flags,
		.albedo = resource_get_handle(res.albedo) ? resource_manager_get_texture(res.albedo).descriptor.handle : 0u,
		.mro = resource_get_handle(res.mro) ? resource_manager_get_texture(res.mro).descriptor.handle : 0u,
		.normalmap = resource_get_handle(res.normalmap) ? resource_manager_get_texture(res.normalmap).descriptor.handle : 0u,
		.emissive = resource_get_handle(res.emissive) ? resource_manager_get_texture(res.emissive).descriptor.handle : 0u,
                .clearcoat = res.clearcoat
        });
}

}

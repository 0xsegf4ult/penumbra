#pragma once

#include <penumbra/resource/geometry.hpp>
#include <penumbra/resource/material.hpp>
#include <penumbra/math/matrix.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/types.hpp>
#include <span>

namespace penumbra
{

struct render_material_data
{
	material_factors factors{};
	u32 flags{0u};

	u32 albedo{0u};
	u32 mro{0u};
	u32 normalmap{0u};
	u32 emissive{0u};

	clearcoat_info clearcoat{};
};

struct renderer_geometry_storage
{
	GPUPointer vertex_pos;
	GPUPointer vertex_uv;
	GPUPointer vertex_nor_tan;
	GPUPointer vertex_skin;
	GPUPointer index;
	GPUPointer cluster;
	GPUPointer lod;
};

struct renderer_skinned_geometry_instance
{
	u32 bone_offset;
	u32 vertex_offset;
	u32 vertex_skinned_offset;
	u32 vertex_count;
};

void renderer_resource_state_init();
void renderer_resource_state_shutdown();
u64 renderer_resource_transfer_syncval();
void renderer_resource_copy_async();

u32 renderer_geometry_write_vertices(const geom_position_format* pos_data, const geom_uv_format* uv_data, const geom_nor_tan_format* nrm_data, u32 count);
u32 renderer_geometry_write_skinned(const geom_skinned_format* data, u32 count);
u32 renderer_geometry_write_indices(const geom_index_format* data, u32 count);
u32 renderer_geometry_write_clusters(const geom_cluster_format* data, u32 count);
u32 renderer_geometry_write_lods(const geom_lod_format* data, u32 count);
void renderer_write_texture(GPUTexture texture, std::span<const u8> data, u32 num_mips, u32 num_layers);
void renderer_write_material(u32 offset, const render_material_data& data);
void renderer_write_bones(u32 offset, const mat4* data, u16 count);

renderer_skinned_geometry_instance renderer_geometry_instantiate_skin(u32 vertex_offset, u32 vertex_count, u16 bone_count);

renderer_geometry_storage renderer_geometry_get_storage();
GPUPointer renderer_material_get_storage();
GPUPointer renderer_bones_get_storage();

}

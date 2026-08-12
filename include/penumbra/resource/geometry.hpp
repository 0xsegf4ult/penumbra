#pragma once

#include <penumbra/math/vector.hpp>
#include <penumbra/types.hpp>
#include <string>

namespace penumbra
{
	
enum VertexFormat : u32 
{
	VERTEX_FORMAT_STATIC,
	VERTEX_FORMAT_SKINNED
};

struct GeometryFileFormat
{
	constexpr static u32 fmt_magic = 0x4c444d4c; //FIXME 
        constexpr static u32 fmt_major = 2u;
        constexpr static u32 fmt_minor = 1u;

	struct Header
        {
                u32 magic{fmt_magic};
                u32 vmajor{fmt_major};
                u32 vminor{fmt_minor};
                VertexFormat vert_format;
                u32 vpos_offset;
                u32 vuv_offset;
                u32 vnorms_offset;
                u32 index_offset;
                u32 lod_offset;
                u32 num_lods;
                u32 cluster_offset;
                vec4 sphere;
        };

	struct LOD
        {
                u32 cluster_offset{0u};
                u32 cluster_count{0u};
        };

        struct Cluster
        {
                s32 vertex_offset{0};
                u32 vertex_count{0u};
                u32 index_offset{0u};
                u32 index_count{0u};

                vec4 sphere;
                vec4 cone;
        };
};

using geom_position_format = vec3;
using geom_uv_format = vec2;
using geom_nor_tan_format = uvec2;
using geom_index_format = u8;

struct geom_cluster_format
{
	s32 vertex_offset;
	u32 vertex_count;
	u32 index_offset;
	u32 index_count;

	vec4 sphere;
	vec4 cone;
};

struct geom_lod_format
{
	u32 cluster_offset{0u};
	u32 cluster_count{0u};
};

struct geom_skinned_format
{
	vec3 pos;
	float enc_tangent;
	vec2 uv;
	Vector<u16, 2> oct_normal;
	u32 joints;
	vec4 weights;
};

struct geometry_import_desc
{
	std::string name;

	const geom_position_format* vertex_pos{nullptr};
	const geom_uv_format* vertex_uv{nullptr};
	const geom_nor_tan_format* vertex_nor_tan{nullptr};
	const geom_skinned_format* vertex_skinned{nullptr};
	const geom_index_format* index{nullptr};
	const geom_cluster_format* cluster{nullptr};
	const geom_lod_format* lod{nullptr};

	u32 vertex_count;
	u32 index_count;
	u32 cluster_count;
       	u32 lod_count;

	vec4 sphere;	
};

struct geometry_resource 
{
	std::string name;
	u32 vertex_offset;
	u32 vertex_count;
	u32 index_offset;
	u32 index_count;
	u32 cluster_offset;
	u32 cluster_count;
	u32 l0_cluster_count;
	u32 lod_offset;
	u32 lod_count;
	vec4 sphere;
	u64 syncval;
	bool skinned_vertex;
};

}

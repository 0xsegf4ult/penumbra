#pragma once

#include <penumbra/resource/rid.hpp>
#include <penumbra/resource/geometry.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/types.hpp>

#include <string>
#include <span>
#include <vector>

namespace penumbra
{

struct geometry_full_vertex
{
	vec3 pos;
	vec3 nrm;
	vec4 tan;
	vec2 uv;
	uvec4 joints;
	vec4 weights;
};

struct geometry_import_context
{
	std::string name;

	std::vector<geometry_full_vertex> vertices;
	std::vector<u32> indices;

	std::vector<geom_position_format> vertex_pos;
	std::vector<geom_uv_format> vertex_uv;
	std::vector<geom_nor_tan_format> vertex_nor_tan;
	std::vector<geom_skinned_format> vertex_skinned;
	std::vector<geom_index_format> index;
	std::vector<geom_cluster_format> cluster;
	std::vector<geom_lod_format> lod;

	bool has_normals{false};
	bool has_tangents{false};
	bool is_skinned{false};
};

enum import_texture_type
{
	IMPORT_TEXTURE_ALBEDO,
	IMPORT_TEXTURE_MRO,
	IMPORT_TEXTURE_NORMALMAP,
	IMPORT_TEXTURE_EMISSIVE,
	IMPORT_TEXTURE_CUBEMAP
};

constexpr const char* texture_type_names[] =
{
	"COLOR",
	"METALROUGHNESS",
	"NORMAL",
	"EMISSIVE",
	"CUBE"
};

ResourceID import_geometry(geometry_import_context& ctx);
ResourceID import_texture(std::string_view name, import_texture_type type, std::span<const u8> data, uvec3 dim);

}

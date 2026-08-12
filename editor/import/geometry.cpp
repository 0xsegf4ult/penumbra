#include <import/resource.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/resource.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/types.hpp>

#include <meshoptimizer.h>

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <vector>

using std::memcpy;

namespace penumbra
{

struct geometry_full_lod
{
	s32 vertex_offset;
	u32 vertex_count;
	u32 index_offset;
	u32 index_count;
};

constexpr u32 geometry_max_lod_count = 8;

const float signNotZero(float v)
{
	return (v >= 0.0f) ? 1.0f : -1.0f;
}

u32 vec3_to_oct_snorm(const vec3& input, [[maybe_unused]]bool highp = false)
{
	Vector<u16, 2> projected;

	const float invl1norm = (1.0f) / (std::abs(input.x) + std::abs(input.y) + std::abs(input.z));

	auto packSnorm16 = [](float f) -> u16 
	{
		return static_cast<u16>(std::round(std::clamp(f, -1.0f, 1.0f) * 32767.0f));
	};

	if(input.z < 0.0f)
	{
		projected.x = packSnorm16((1.0f - std::abs(input.y * invl1norm)) * signNotZero(input.x));
		projected.y = packSnorm16((1.0f - std::abs(input.x * invl1norm)) * signNotZero(input.y));
	}
	else
	{
		projected.x = packSnorm16(input.x * invl1norm);
		projected.y = packSnorm16(input.y * invl1norm);
	}

	return std::bit_cast<u32>(projected);
}

float encode_diamond(const vec2& p)
{
	const float x = p.x / (std::abs(p.x) + std::abs(p.y));

	auto sgn = [](float v) -> float
	{
		return (v > 0.0f) - (v < 0.0f);
	};

	const float py_sign = sgn(p.y);
	return -py_sign * 0.25f * x + 0.5f + py_sign * 0.25f;
}

u32 encode_tangent(const vec3& normal, const vec3& tangent, bool flip)
{
	vec3 t1;
	if(std::abs(normal.y) > std::abs(normal.z))
		t1 = vec3{normal.y, -normal.x, 0.0f};
	else
		t1 = vec3{normal.z, 0.0f, -normal.x};

	const vec3 nt1 = vec3::normalize(t1);

	const vec3 t2 = vec3::cross(nt1, normal);
	const vec2 packed_tangent = vec2(vec3::dot(tangent, nt1), vec3::dot(tangent, t2));
	const float diamond = encode_diamond(packed_tangent);

	u32 fbits = std::bit_cast<u32>(diamond);
	if(flip)
		fbits |= 1u;
	else
		fbits &= (~1u);

	return fbits;
}

ResourceID import_geometry(geometry_import_context& ctx)
{
	std::vector<geometry_full_vertex> remap_vertices;
	std::vector<u32> remap_table(ctx.indices.size());
	size_t vertex_count = meshopt_generateVertexRemap(remap_table.data(), ctx.indices.data(), ctx.indices.size(), ctx.vertices.data(), ctx.vertices.size(), sizeof(geometry_full_vertex));

	meshopt_remapIndexBuffer(ctx.indices.data(), ctx.indices.data(), ctx.indices.size(), remap_table.data());
	remap_vertices.resize(vertex_count);
	meshopt_remapVertexBuffer(remap_vertices.data(), ctx.vertices.data(), ctx.vertices.size(), sizeof(geometry_full_vertex), remap_table.data());

	meshopt_optimizeVertexCache(ctx.indices.data(), ctx.indices.data(), ctx.indices.size(), remap_vertices.size());
	meshopt_optimizeOverdraw(ctx.indices.data(), ctx.indices.data(), ctx.indices.size(), &remap_vertices[0].pos.x, remap_vertices.size(), sizeof(geometry_full_vertex), 1.01f);
	meshopt_optimizeVertexFetch(remap_vertices.data(), ctx.indices.data(), ctx.indices.size(), remap_vertices.data(), remap_vertices.size(), sizeof(geometry_full_vertex));

	std::array<geometry_full_lod, geometry_max_lod_count> lods;
       	lods[0] = {0, static_cast<u32>(remap_vertices.size()), 0u, static_cast<u32>(ctx.indices.size())};
	u32 num_lods = 1;

	std::vector<geometry_full_vertex> tmplod_vtx(lods[0].vertex_count);
	std::vector<u32> tmplod(lods[0].index_count);

	s32 voffset = lods[0].vertex_count;
	u32 offset = lods[0].index_count;

	float vweights[3] = {0.5f, 0.5f, 0.5f};

	for(u32 l = 1; l < geometry_max_lod_count; l++)
	{
		if(ctx.is_skinned)
			break;

		tmplod_vtx.clear();
		tmplod.clear();
		u32 options = 0;
		const float target_error = 1e-2f;
		size_t target_index_count = (size_t(double(lods[l - 1].index_count) * 0.65) / 3) * 3;
		if(target_index_count < 128)
			options = meshopt_SimplifyLockBorder;

		float lod_error = 0.0f;
		u32 lod_size = meshopt_simplifyWithAttributes(tmplod.data(), ctx.indices.data() + lods[l - 1].index_offset, lods[l - 1].index_count, &remap_vertices[lods[l - 1].vertex_offset].pos.x, lods[l - 1].vertex_count, sizeof(geometry_full_vertex), &remap_vertices[lods[l - 1].vertex_offset].nrm.x, sizeof(geometry_full_vertex), vweights, 3, nullptr, target_index_count, target_error, options, &lod_error);	
		if(lods[l - 1].index_count == lod_size || lod_size == 0)
			break;

		if(lod_size >= size_t(double(lods[l - 1].index_count) * 0.95))
			break;

		u32 vtx_size = meshopt_optimizeVertexFetch(tmplod_vtx.data(), tmplod.data(), lod_size, &remap_vertices[lods[l - 1].vertex_offset].pos.x, lods[l - 1].vertex_count, sizeof(geometry_full_vertex));
		lods[l] = {voffset, vtx_size, offset, lod_size};
		voffset += vtx_size;
		offset += lod_size;
		remap_vertices.insert(remap_vertices.end(), tmplod_vtx.begin(), tmplod_vtx.begin() + vtx_size);
		ctx.indices.insert(ctx.indices.end(), tmplod.begin(), tmplod.begin() + lod_size);
		num_lods++;
	}

	std::array<std::vector<meshopt_Meshlet>, geometry_max_lod_count> lod_clusters;
	std::array<std::vector<u32>, geometry_max_lod_count> lod_cluster_vertices;
	std::array<std::vector<u8>, geometry_max_lod_count> lod_cluster_triangles;
	std::array<u32, geometry_max_lod_count> lod_cluster_counts;

	const u32 max_vertices = 64u;
	const u32 max_triangles = 96u;
	const float cone_weight = 0.25f;

	for(u32 l = 0; l < num_lods; l++)
	{
		auto ccount = meshopt_buildMeshletsBound(lods[l].index_count, max_vertices, max_triangles);
		lod_clusters[l].resize(ccount);
		lod_cluster_vertices[l].resize(ccount * max_vertices);
		lod_cluster_triangles[l].resize(ccount * max_triangles * 3);

		ccount = meshopt_buildMeshlets(lod_clusters[l].data(), lod_cluster_vertices[l].data(), lod_cluster_triangles[l].data(), &ctx.indices[lods[l].index_offset], lods[l].index_count, &remap_vertices[lods[l].vertex_offset].pos.x, lods[l].vertex_count, sizeof(geometry_full_vertex), max_vertices, max_triangles, cone_weight);

		lod_cluster_counts[l] = ccount;

		for(u32 m = 0; m < ccount; m++)
		{
			auto& meshlet = lod_clusters[l][m];
			meshopt_optimizeMeshlet(&lod_cluster_vertices[l][meshlet.vertex_offset], &lod_cluster_triangles[l][meshlet.triangle_offset], meshlet.triangle_count, meshlet.vertex_count);
		}
	}
	
	u32 exp_vcount = 0;
	u32 exp_icount = 0;
	u32 exp_ccount = 0;
	
	for(u32 l = 0; l < num_lods; l++)
	{
		for(u32 m = 0; m < lod_cluster_counts[l]; m++)
		{
			auto& meshlet = lod_clusters[l][m];
			for(u32 i = 0; i < meshlet.vertex_count; i++)
			{
				u32 vidx = lod_cluster_vertices[l][meshlet.vertex_offset + i] + lods[l].vertex_offset;
				bool flipn = remap_vertices[vidx].tan.w < 0.0f;

				geom_nor_tan_format nor_tan{};
				nor_tan.x = vec3_to_oct_snorm(remap_vertices[vidx].nrm, false);
				nor_tan.y = encode_tangent(remap_vertices[vidx].nrm, remap_vertices[vidx].tan.demote<3>(), flipn);
				u32 voff;
				
				if(ctx.is_skinned)
				{
					Vector<u16, 2> oct_normal;
					memcpy(&oct_normal, &nor_tan.x, sizeof(u32));
					u32 joints = ((remap_vertices[vidx].joints[3] & 0xFF) << 24) |
							  ((remap_vertices[vidx].joints[2] & 0xFF) << 16) |
							  ((remap_vertices[vidx].joints[1] & 0xFF) << 8) | 
							  (remap_vertices[vidx].joints[0] & 0xFF);
					
					ctx.vertex_skinned.push_back
					({
						remap_vertices[vidx].pos,
						std::bit_cast<float>(nor_tan.y),
						remap_vertices[vidx].uv,
						oct_normal,
						joints,
						remap_vertices[vidx].weights
					});
				}
				else
				{
					ctx.vertex_pos.push_back(remap_vertices[vidx].pos);
					ctx.vertex_uv.push_back(remap_vertices[vidx].uv);
					ctx.vertex_nor_tan.push_back(nor_tan);
				}
			}

			ctx.index.insert(ctx.index.end(), &lod_cluster_triangles[l][meshlet.triangle_offset], &lod_cluster_triangles[l][meshlet.triangle_offset] + meshlet.triangle_count * 3);

			auto bounds = meshopt_computeMeshletBounds(&lod_cluster_vertices[l][meshlet.vertex_offset], &lod_cluster_triangles[l][meshlet.triangle_offset], meshlet.triangle_count, &remap_vertices[lods[l].vertex_offset].pos.x, lods[l].vertex_count, sizeof(geometry_full_vertex));

			ctx.cluster.push_back
			({
				s32(exp_vcount),
				meshlet.vertex_count,
				exp_icount,
				meshlet.triangle_count * 3,
				vec4{bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius},
				vec4{bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2], bounds.cone_cutoff}
			});

			exp_vcount += meshlet.vertex_count;
			exp_icount += meshlet.triangle_count * 3;
		}

		ctx.lod.push_back	
		({
			exp_ccount,
			lod_cluster_counts[l]
		});

		exp_ccount += lod_cluster_counts[l];	
	}

	auto mbounds = meshopt_computeSphereBounds(&remap_vertices[0].pos.x, lods[0].vertex_count, sizeof(geometry_full_vertex), nullptr, 0);
	vec4 msphere = vec4{mbounds.center[0], mbounds.center[1], mbounds.center[2], mbounds.radius};
	return resource_manager_import_geometry
	({
		ctx.name,
	 	ctx.is_skinned ? nullptr : ctx.vertex_pos.data(),
		ctx.is_skinned ? nullptr : ctx.vertex_uv.data(),
		ctx.is_skinned ? nullptr : ctx.vertex_nor_tan.data(),
		ctx.is_skinned ? ctx.vertex_skinned.data() : nullptr,
		ctx.index.data(),
		ctx.cluster.data(),
		ctx.lod.data(),
		static_cast<u32>(ctx.is_skinned ? ctx.vertex_skinned.size() : ctx.vertex_pos.size()),
		static_cast<u32>(ctx.index.size()),
		static_cast<u32>(ctx.cluster.size()),
		static_cast<u32>(ctx.lod.size()),
		msphere,
	});
}

}

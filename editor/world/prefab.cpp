#include <world/prefab.hpp>
#include <world/state.hpp>
#include <world/components/render.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/ecs.hpp>
#include <penumbra/log.hpp>
#include <penumbra/resource.hpp>
#include <penumbra/vfs.hpp>
#include <penumbra/types.hpp>

#include <cassert>
#include <format>
#include <map>
#include <vector>

namespace penumbra
{

void load_prefab(WorldState& world, const vfs_path& path)
{
	auto& graph = world.entities;

	auto pfile = vfs_open(path, VFS_ACCESS_READ);
	if(pfile < 0)
	{
		log::error("editor: failed loading prefab [{}]: could not open file", path.string()); 
		return;
	}

	const auto* pdata = vfs_map(pfile);
	const auto* header = reinterpret_cast<const PrefabFileFormat::Header*>(pdata);

	if(header->magic != PrefabFileFormat::fmt_magic || header->vmajor != PrefabFileFormat::fmt_major)
	{
		log::error("editor: failed loading prefab [{}]: file is invalid", path.string());
		vfs_close(pfile);
		return;
	}

	auto read_string_table = [&pdata, &header](u32 offset) -> const char*
	{
		return reinterpret_cast<const char*>(pdata + header->string_table_offset + offset);
	};
	const auto* mtl_table = reinterpret_cast<const PrefabFileFormat::Material*>(pdata + header->material_table_offset);
	std::map<u32, ResourceID> mtl_map;

	for(u32 i = 0; i < header->material_table_count; i++)
	{
		const PrefabFileFormat::Material& mtl = mtl_table[i];

		mtl_map[i] = resource_manager_import_material
		({
			.name = (mtl.name > 0) ? read_string_table(mtl.name) : std::format("material_pbr_{}", i),
			.factors = 
			{
				.albedo = vec4{mtl.diffuse_factor, 1.0f},
				.roughness = mtl.roughness_factor,
				.metallic = mtl.metallic_factor,
				.normal = mtl.normal_factor,
				.reflectivity = mtl.reflectivity,
				.alpha_cf = mtl.alpha_cf,
				.emissive = mtl.emissive_factor,
			},
			.flags = mtl.flags,
			.albedo = mtl.albedo > 0 ? resource_manager_load_texture(vfs_path{"textures"} / read_string_table(mtl.albedo)) : ResourceID{},
			.mro = mtl.mro > 0 ? resource_manager_load_texture(vfs_path{"textures"} / read_string_table(mtl.mro)) : ResourceID{},
			.normalmap = mtl.normalmap > 0 ? resource_manager_load_texture(vfs_path{"textures"} / read_string_table(mtl.normalmap)) : ResourceID{},
			.emissive = mtl.emissive > 0 ? resource_manager_load_texture(vfs_path{"textures"} / read_string_table(mtl.emissive)) : ResourceID{},
		});
		
	}

	const auto* node_table = reinterpret_cast<const PrefabFileFormat::Node*>(pdata + header->node_table_offset);

	ecs::entity root_entity = world.spawn(path.string());
	add_entity_as_child(graph, world.root, root_entity);

	std::vector<ecs::entity> node_map(header->node_table_count + 1);
	std::vector<mat4> node_matrix_world(header->node_table_count + 1);
	node_map[0] = root_entity;
	node_matrix_world[0] = mat4::identity();

	for(u32 i = 0; i < header->node_table_count; i++)
	{
		const PrefabFileFormat::Node& node = node_table[i];

		ecs::entity node_ent = world.spawn(node.name > 0 ? read_string_table(node.name) : std::format("node_{}", i));

		Transform ntx = Transform{node.translation, node.rotation, node.scale};
		mat4 matrix_world = ntx.as_matrix() * node_matrix_world[node.parent];
		node_matrix_world[i + 1] = matrix_world;	

		graph.emplace_or_replace<Transform>(node_ent, ntx);
		add_entity_as_child(graph, node_map[node.parent], node_ent);
		node_map[i + 1] = node_ent;

		u32 c_passed = 0;
		const auto* cptr = pdata + node.components_offset;
		while(c_passed < std::min(node.component_count, 64u))
		{
			c_passed++;
			const auto* cmp = reinterpret_cast<const PrefabFileFormat::Component*>(cptr);
			switch(cmp->type)
			{
			case PrefabFileFormat::ComponentType::STATIC_MESH:	
			case PrefabFileFormat::ComponentType::SKINNED_MESH:
			{
				auto is_skinned_cmp = (cmp->type == PrefabFileFormat::ComponentType::SKINNED_MESH);
				const auto* smc = reinterpret_cast<const PrefabFileFormat::StaticMeshComponent*>(cptr);
				ResourceID geom;
				if(smc->mesh)
				{
					geom = resource_manager_load_geometry(vfs_path{"meshes"} / read_string_table(smc->mesh));
				}

				ResourceID material{0u};
				if(smc->material)
				{
					material = mtl_map[smc->material - 1];
				}

				ResourceID skeleton{0u};
				if(is_skinned_cmp)
				{
					const auto* skc = reinterpret_cast<const PrefabFileFormat::SkinnedMeshComponent*>(cptr);
					skeleton = resource_manager_load_skeleton(vfs_path{"anim"} / read_string_table(skc->skeleton));
				}

				u32 shadow_levels = 3;
				if(smc->flags & PREFAB_MESH_NO_SHADOWCAST_C3)
					shadow_levels = 3;
				if(smc->flags & PREFAB_MESH_NO_SHADOWCAST_C2)
					shadow_levels = 2;
				if(smc->flags & PREFAB_MESH_NO_SHADOWCAST_C1)
					shadow_levels = 1;
				if(smc->flags & PREFAB_MESH_NO_SHADOWCAST)
					shadow_levels = 0;

				auto& geom_data = resource_manager_get_geometry(geom);
				assert(geom_data.skinned_vertex == is_skinned_cmp);
				
				auto rd_object = renderer_world_insert_object
				({
					matrix_world,
					geom,
					material,
					skeleton
				}, shadow_levels);

				graph.emplace<render_object_component>(node_ent, geom, material, rd_object);
				if(is_skinned_cmp && resource_get_handle(skeleton))
					graph.emplace<render_skeleton_component>(node_ent, skeleton);

				cptr += is_skinned_cmp ? sizeof(PrefabFileFormat::SkinnedMeshComponent) : sizeof(PrefabFileFormat::StaticMeshComponent);
				break;
			}
			default:
				log::warn("load_prefab: loading invalid component type");
				cptr += sizeof(PrefabFileFormat::Component);
			}
		}
	}

	vfs_close(pfile);
}

}


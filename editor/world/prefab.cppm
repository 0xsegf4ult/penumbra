export module penumbra.editor:prefab;

import :world_state;

import penumbra.core;
import penumbra.math;
import penumbra.ecs;
import penumbra.renderer;
import penumbra.resource;
import std;
using std::uint32_t;

namespace penumbra
{

struct PrefabFileFormat
{
	constexpr static uint32_t fmt_magic = 0x4246504c;
        constexpr static uint32_t fmt_major_version = 2u;
        constexpr static uint32_t fmt_minor_version = 1u;

        struct Header
        {
                uint32_t magic{fmt_magic};
                uint32_t vmajor{fmt_major_version};
                uint32_t vminor{fmt_minor_version};
                uint32_t string_table_offset;
                uint32_t material_table_offset;
                uint32_t material_table_count;
                uint32_t node_table_offset;
                uint32_t node_table_count;
        };

	struct Node
        {
                uint32_t name;
                uint32_t parent;
                vec3 translation;
                Quaternion rotation;
                vec3 scale;
                uint32_t component_count;
                uint32_t components_offset;
        };

        enum class ComponentType : uint32_t
        {
                INVALID = 0,
                STATIC_MESH = 1,
                SKINNED_MESH = 2,
                DIRLIGHT = 3,
                SPOTLIGHT = 4,
                POINTLIGHT = 5
        };

        struct Component
        {
                ComponentType type;
        };

	struct StaticMeshComponent : public Component
	{
		uint32_t flags;
		uint32_t mesh;
		uint32_t material;
	};
};

export void load_prefab(WorldState& world, const vfs::path& path)
{
	auto& graph = world.entities;

	auto pfile = vfs::open(path, vfs::access_readonly);
	if(!pfile.has_value())
	{
		log::error("editor: failed loading prefab: {}", vfs::file_open_error_to_string(pfile.error()));
		return;
	}

	const auto* pdata = vfs::map<std::byte>(*pfile, vfs::access_readonly);
	const auto* header = reinterpret_cast<const PrefabFileFormat::Header*>(pdata);

	if(header->magic != PrefabFileFormat::fmt_magic || header->vmajor != PrefabFileFormat::fmt_major_version)
	{
		log::error("editor: failed loading prefab: file is invalid");
		return;
	}

	auto read_string_table = [&pdata, &header](uint32_t offset) -> const char*
	{
		return reinterpret_cast<const char*>(pdata + header->string_table_offset + offset);
	};

	const auto* node_table = reinterpret_cast<const PrefabFileFormat::Node*>(pdata + header->node_table_offset);

	ecs::entity root_entity = world.spawn(path.string());
	add_entity_as_child(graph, world.root, root_entity);

	std::map<uint32_t, ecs::entity> node_map;
	node_map[0] = root_entity;

	for(uint32_t i = 0; i < header->node_table_count; i++)
	{
		const PrefabFileFormat::Node& node = node_table[i];

		ecs::entity node_ent = world.spawn(node.name > 0 ? read_string_table(node.name) : std::format("node_{}", i));

		Transform ntx = Transform{node.translation, node.rotation, node.scale} * graph.get<Transform>(node_map[node.parent]);

		graph.emplace<Transform>(node_ent, ntx);
		add_entity_as_child(graph, node_map[node.parent], node_ent);
		node_map[i + 1] = node_ent;

		uint32_t c_passed = 0;
		const auto* cptr = pdata + node.components_offset;
		while(c_passed < std::min(node.component_count, 64u))
		{
			c_passed++;
			const auto* cmp = reinterpret_cast<const PrefabFileFormat::Component*>(cptr);
			switch(cmp->type)
			{
			case PrefabFileFormat::ComponentType::STATIC_MESH:
			{
				const auto* smc = reinterpret_cast<const PrefabFileFormat::StaticMeshComponent*>(cptr);
				ResourceID geom;
				if(smc->mesh)
				{
					geom = resource_manager_load_geometry(vfs::path{"meshes"} / read_string_table(smc->mesh));
				}

				auto& geom_data = resource_manager_get_geometry(geom);
				auto rd_object = renderer_world_insert_object
				({
				 	ntx,
					RENDER_BUCKET_DEFAULT,
					geom_data.l0_cluster_count,
					geom_data.lod_offset,
					geom_data.lod_count
				});

				cptr += sizeof(PrefabFileFormat::StaticMeshComponent);
				break;
			}
			default:
				log::warn("load_prefab: loading invalid component type");
				cptr += sizeof(PrefabFileFormat::Component);
			}
		}
	}
}

}


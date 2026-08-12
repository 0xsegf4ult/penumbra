#pragma once

#include <penumbra/math/transform.hpp>
#include <penumbra/vfs.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct WorldState;

enum PrefabMeshFlags
{
	PREFAB_MESH_NO_SHADOWCAST = 0x1, 
	PREFAB_MESH_NO_SHADOWCAST_C1 = 0x2,
	PREFAB_MESH_NO_SHADOWCAST_C2 = 0x4,
	PREFAB_MESH_NO_SHADOWCAST_C3 = 0x8
};

struct PrefabFileFormat
{
	constexpr static u32 fmt_magic = 0x4246504c;
        constexpr static u32 fmt_major = 2u;
        constexpr static u32 fmt_minor = 1u;

        struct Header
        {
                u32 magic{fmt_magic};
                u32 vmajor{fmt_major};
                u32 vminor{fmt_minor};
                u32 string_table_offset;
                u32 material_table_offset;
                u32 material_table_count;
                u32 node_table_offset;
                u32 node_table_count;
        };

	struct Material
	{
		u32 name;

		vec3 diffuse_factor;
		float roughness_factor;
		float metallic_factor;
		float normal_factor;
		float reflectivity;
		float alpha_cf;
		vec3 emissive_factor;
		u32 flags;

		u32 albedo;
		u32 mro;
		u32 normalmap;
		u32 emissive;
	};

	struct Node
        {
                u32 name;
                u32 parent;
                vec3 translation;
                Quaternion rotation;
                vec3 scale;
                u32 component_count;
                u32 components_offset;
        };

        enum class ComponentType : u32
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
		u32 flags;
		u32 mesh;
		u32 material;
	};

	struct SkinnedMeshComponent : public StaticMeshComponent
	{
		u32 skeleton;
	};
};

void load_prefab(WorldState& world, const vfs_path& path);

}

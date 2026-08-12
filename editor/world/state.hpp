#pragma once

#include <world/envmap.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/ecs.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/types.hpp>
#include <string>

namespace penumbra
{

using entity_name = std::string;

struct entity_relationship
{
	ecs::entity parent{ecs::null};
	ecs::entity first_child{ecs::null};
	ecs::entity prev_sibling{ecs::null};
	ecs::entity next_sibling{ecs::null};
};

void add_entity_as_child(ecs::registry& graph, ecs::entity parent, ecs::entity child);
void unlink_entity(ecs::registry& graph, ecs::entity entity);

struct transform_dirty_t{};
mat4 get_entity_world_matrix(ecs::registry& graph, ecs::entity entity);

struct WorldState
{
	WorldState();
	~WorldState();
	ecs::entity spawn(std::string_view name);
	void set_envmap(const EnvironmentMap& envmap);
	void update_transforms();
	void update_entity_subtree(ecs::entity entity, mat4 matrix_world);

	ecs::registry entities;
	ecs::entity root;
	ecs::entity selected_entity{ecs::null};
	ecs::entity main_camera;
	ecs::entity env;
	render_environment_map r_envmap;
};

}

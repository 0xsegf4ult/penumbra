#include <world/state.hpp>
#include <world/envmap.hpp>
#include <world/components/camera.hpp>
#include <world/components/render.hpp>
#include <world/components/lights.hpp>
#include <penumbra/ecs.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/resource.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/types.hpp>
#include <tracy/Tracy.hpp>
#include <string>
#include <cassert>

namespace penumbra
{

void add_entity_as_child(ecs::registry& graph, ecs::entity parent, ecs::entity child)
{
	entity_relationship& re = graph.get<entity_relationship>(parent);
	if(!graph.valid(re.first_child))
	{
		re.first_child = child;
	}
	else
	{
		ecs::entity cur = re.first_child;
		for(;;)
		{
			const auto& er = graph.get<entity_relationship>(cur);
			if(graph.valid(er.next_sibling))
				cur = er.next_sibling;
			else
				break;
		}

		graph.get<entity_relationship>(cur).next_sibling = child;
		graph.get<entity_relationship>(child).prev_sibling = cur;
	}

	graph.get<entity_relationship>(child).parent = parent;
}

void unlink_entity(ecs::registry& graph, ecs::entity entity)
{
	entity_relationship& re = graph.get<entity_relationship>(entity);
	if(graph.valid(re.prev_sibling))
	{
		graph.get<entity_relationship>(re.prev_sibling).next_sibling = re.next_sibling;
		return;
	}

	if(graph.valid(re.parent))
	{
		auto& p_re = graph.get<entity_relationship>(re.parent);
		if(p_re.first_child == entity)
			p_re.first_child = re.next_sibling;			
	}
}

mat4 get_entity_world_matrix(ecs::registry& graph, ecs::entity entity)
{
	ZoneScoped;

	entity_relationship& re = graph.get<entity_relationship>(entity);
	Transform& tx = graph.get<Transform>(entity);
	if(graph.valid(re.parent))
		return tx.as_matrix() * get_entity_world_matrix(graph, re.parent);
	else
		return tx.as_matrix();
}

WorldState::WorldState()
{
	root = spawn("root");

	main_camera = spawn("main_camera");
	add_entity_as_child(entities, root, main_camera);
	entities.emplace<camera_component>(main_camera);

	env = spawn("env");
	add_entity_as_child(entities, root, env);
	entities.emplace<directional_light_component>(env, vec3{-0.14f, -0.3f, -0.3f}, vec3{0.68f, 0.53f, 0.46f}, 38000.0f);

	auto envmap = load_envmap("hdri/kloppenheim");
	set_envmap(envmap);
}

WorldState::~WorldState()
{
}

ecs::entity WorldState::spawn(std::string_view name)
{
	auto ent = entities.create();
	entities.emplace<entity_name>(ent, name);
	entities.emplace<entity_relationship>(ent);
	entities.emplace<Transform>(ent);
	return ent;
}

void WorldState::set_envmap(const EnvironmentMap& envmap)
{
	r_envmap.irradiance = resource_manager_get_texture(envmap.irradiance).descriptor;
	r_envmap.prefiltered = resource_manager_get_texture(envmap.prefiltered).descriptor;
}

void WorldState::update_transforms()
{
	for(auto [entity] : entities.view<transform_dirty_t>().each())
	{
		auto wm = get_entity_world_matrix(entities, entity);
		update_entity_subtree(entity, wm);
		entities.remove<transform_dirty_t>(entity);
	}
}

void WorldState::update_entity_subtree(ecs::entity entity, mat4 matrix_world)
{
	auto* robj = entities.try_get<render_object_component>(entity);
	if(robj)
	{
		renderer_world_update_object(robj->renderer_objectID, matrix_world);
	}

	entity_relationship& re = entities.get<entity_relationship>(entity);
	ecs::entity child = re.first_child;
	while(entities.valid(child))
	{
		update_entity_subtree(child, entities.get<Transform>(child).as_matrix() * matrix_world);
		child = entities.get<entity_relationship>(child).next_sibling;
	}
}

}

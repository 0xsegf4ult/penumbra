export module penumbra.editor:world_state;
import :camera_component;
import :light_components;

import penumbra.ecs;
import penumbra.math;
import std;

namespace penumbra
{

export using entity_name = std::string;

export struct entity_relationship
{
	ecs::entity parent{ecs::null};
	ecs::entity first_child{ecs::null};
	ecs::entity prev_sibling{ecs::null};
	ecs::entity next_sibling{ecs::null};
};

export void add_entity_as_child(ecs::registry& graph, ecs::entity parent, ecs::entity child)
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

export struct WorldState
{
	WorldState()
	{
		root = spawn("root");

		main_camera = spawn("main_camera");
		add_entity_as_child(entities, root, main_camera);
		entities.emplace<camera_component>(main_camera);

		env = spawn("env");
		add_entity_as_child(entities, root, env);
		entities.emplace<directional_light_component>(env, vec3{-0.14f, -0.3f, -0.3f}, vec3{0.68f, 0.53f, 0.46f}, 38000.0f);
	}

	ecs::entity spawn(std::string_view name)
	{
		auto ent = entities.create();
		entities.emplace<entity_name>(ent, name);
		entities.emplace<entity_relationship>(ent);
		entities.emplace<Transform>(ent);
		return ent;
	}

	ecs::registry entities;
	ecs::entity root;
	ecs::entity selected_entity{ecs::null};
	ecs::entity main_camera;
	ecs::entity env;
};

}

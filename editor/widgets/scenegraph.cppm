export module penumbra.editor:scenegraph;

import :widget;
import :world_state;

import penumbra.ecs;
import imgui;
import std;

using std::uint32_t;

namespace penumbra
{

export class ScenegraphView : public Widget
{
public:
	ScenegraphView(WorldState* ws) : Widget("World"), world{ws} {}

	void on_draw() override
	{
		if(!world)
			return;

		uint32_t ctr = 0;
		ecs::entity cur = world->entities.get<entity_relationship>(world->root).first_child;
		while(world->entities.valid(cur))
		{
			tree_draw(cur, ctr);
			cur = world->entities.get<entity_relationship>(cur).next_sibling;
		}
	}
private:
	void tree_draw(ecs::entity ent, uint32_t& ctr)
	{
		auto& graph = world->entities;

		ImGui::PushID(ctr);
		ctr++;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;

		if(ctr != 0 && !graph.valid(graph.get<entity_relationship>(ent).first_child))
			flags |= ImGuiTreeNodeFlags_Leaf;

		if(world->selected_entity == ent)
			flags |= ImGuiTreeNodeFlags_Selected;

		if(ImGui::TreeNodeEx(graph.get<entity_name>(ent).c_str(), flags))
		{
			if(ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
				world->selected_entity = ent;

			ecs::entity cur = graph.get<entity_relationship>(ent).first_child;
			while(graph.valid(cur))
			{
				tree_draw(cur, ctr);
				cur = graph.get<entity_relationship>(cur).next_sibling;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	WorldState* world{nullptr};
};

}

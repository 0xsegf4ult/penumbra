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

		if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
			world->selected_entity = ecs::null;

		if(ImGui::BeginPopupContextWindow("Create", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if(ImGui::MenuItem("Entity"))
			{
				auto ent = world->spawn("Entity");
				add_entity_as_child(world->entities, (world->selected_entity != ecs::null) ? world->selected_entity : world->root, ent);
			}

			ImGui::EndPopup();
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

		bool node_open = ImGui::TreeNodeEx(graph.get<entity_name>(ent).c_str(), flags);
		
		if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
			world->selected_entity = ent;

		bool wants_delete = false;
		if(ImGui::BeginPopupContextItem())
		{
			bool is_protected = (ent == world->main_camera) || (ent == world->env);
			ImGui::BeginDisabled(is_protected);
			if(ImGui::MenuItem("Delete"))
				wants_delete = true;
			ImGui::EndDisabled();

			ImGui::EndPopup();
		}

		if(node_open)
		{
			ecs::entity cur = graph.get<entity_relationship>(ent).first_child;
			while(graph.valid(cur))
			{
				tree_draw(cur, ctr);
				cur = graph.get<entity_relationship>(cur).next_sibling;
			}

			ImGui::TreePop();
		}

		if(wants_delete)
		{
			if(world->selected_entity == ent)
				world->selected_entity = ecs::null;

			//graph.destroy(ent);
		}

		ImGui::PopID();
	}

	WorldState* world{nullptr};
};

}

export module penumbra.editor:inspector;

import :camera_component;
import :light_components;
import :render_object_component;
import :widget;
import :world_state;

import penumbra.ecs;
import penumbra.resource;
import penumbra.renderer;
import imgui;
import std;

using std::uint32_t;

namespace penumbra
{

bool inspect_float(const char* title, float& flt, vec2 range = vec2{0.0f})
{
	ImGui::PushID(title);

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, 100);
	ImGui::Text("%s", title);

	ImGui::NextColumn();
	ImGui::SetNextItemWidth(-1.0f);

	bool dirty = false;
	if(ImGui::DragFloat("##float", &flt, 0.1f, range.x, range.y, "%.2f"))
		dirty = true;

	ImGui::Columns(1);
	ImGui::PopID();
	return dirty;
}

bool inspect_vec3(const char* title, vec3& vector, bool is_unit = false)
{
	ImGui::PushID(title);

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, 100);
	ImGui::Text("%s", title);

	ImGui::NextColumn();
	ImGui::SetNextItemWidth(-1.0f);

	bool dirty = false;
	if(ImGui::DragFloat3("##vec3", &vector.x, 0.1f, is_unit ? -1.0f : 0.0f, is_unit ? 1.0f : 0.0f, "%.2f"))
		dirty = true;

	ImGui::Columns(1);
	ImGui::PopID();
	return dirty;
}

bool inspect_quat(const char* title, Quaternion& quat)
{
	ImGui::PushID(title);

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, 100);
	ImGui::Text("%s", title);

	ImGui::NextColumn();
	ImGui::SetNextItemWidth(-1.0f);

	bool dirty = false;
	if(ImGui::DragFloat4("##quat", &quat.x, 0.1f, -1.0f, 1.0f, "%.2f"))
		dirty = true;

	ImGui::Columns(1);
	ImGui::PopID();
	return dirty;
}

export class Inspector : public Widget
{
public:
	Inspector(WorldState* ws) : Widget("Inspector"), world{ws} {}

	void on_draw() override
	{
		if(!world)
			return;

		auto& graph = world->entities;
		if(!graph.valid(world->selected_entity))
			return;

		auto& name = graph.get<entity_name>(world->selected_entity);
		ImGuiInputTextFlags tflags = ImGuiInputTextFlags_EnterReturnsTrue;

		ImGui::InputText("", &name, tflags);
		ImGui::SameLine();

		ImGui::Button("Add component");

		inspect_transform(graph.get<Transform>(world->selected_entity));

		auto* camera = graph.try_get<camera_component>(world->selected_entity);
		if(camera)
			inspect_camera(*camera);

		auto* dlight = graph.try_get<directional_light_component>(world->selected_entity);
		if(dlight)
			inspect_directional_light(*dlight);

		auto* robj = graph.try_get<render_object_component>(world->selected_entity);
		if(robj)
		{
			if(robj->material.get_handle())
				inspect_material(robj->material);
		}
	}
private:
	void inspect_transform(Transform& transform)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		if(ImGui::CollapsingHeader("Transform", flags))
		{
			bool dirty = false;
			dirty |= inspect_vec3("Translation", transform.translation);
			dirty |= inspect_quat("Rotation", transform.rotation);
			dirty |= inspect_vec3("Scale", transform.scale);
		}
		ImGui::PopStyleVar();
	}

	void inspect_camera(camera_component& camera)
	{
		if(ImGui::CollapsingHeader("Camera"))
		{
			ImGui::DragFloat("Aperture", &camera.aperture, 1.0f, 1.0f, 48.0f, "f/%.1f");
			ImGui::DragFloat("Shutter speed", &camera.shutter_speed, 1.0f, 1.0f, 4000.0f, "1/%.fs");
			ImGui::DragFloat("ISO", &camera.iso, 1.0f, 100.0f, 1600.0f);
			ImGui::DragFloat("Exposure compensation", &camera.exposure_compensation, 0.0f, -16.0f, 16.0f, "%.fEV");

			ImGui::Combo("Projection", reinterpret_cast<int*>(&camera.projection), "Perspective\0Orthographic\0\0");
			ImGui::DragFloat("Vertical FOV", &camera.vertical_fov, 0.1f, 45.0f, 120.0f);
			ImGui::DragFloat("Near plane", &camera.near_plane, 0.01f, 0.01f, 1.0f);
			ImGui::DragFloat("Far plane", &camera.far_plane, 0.1f);
		}
	}

	void inspect_directional_light(directional_light_component& light)
	{
		if(ImGui::CollapsingHeader("Directional light"))
		{
			ImGui::DragFloat3("Direction", &light.direction.x, 0.01f, -1.0f, 1.0f);
			ImGui::ColorEdit3("Color", &light.color.x);
			ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 0.0f, "%.0flx");
		}
	}

	void inspect_material(const ResourceID& rid)
	{
		auto& mtl = resource_manager_get_material(rid);

		if(ImGui::CollapsingHeader("Material"))
		{
			bool dirty = false;
			
			ImGui::Text("%s", mtl.name.c_str());

			dirty |= ImGui::DragFloat3("Diffuse factor", &mtl.factors.diffuse.x, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::DragFloat("Roughness factor", &mtl.factors.roughness, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::DragFloat("Metallic factor", &mtl.factors.metallic, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::DragFloat("Normal factor", &mtl.factors.normal, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::DragFloat("Reflectivity", &mtl.factors.reflectivity, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::ColorEdit3("Emissive color", &mtl.factors.emissive.x);

			if(mtl.flags & RENDER_MATERIAL_ALPHA_MASK)
				ImGui::Text("MATERIAL_FLAG_ALPHA_MASK");

			if(dirty)
				resource_manager_sync_material(rid);
		}
	}

	WorldState* world{nullptr};
};

}

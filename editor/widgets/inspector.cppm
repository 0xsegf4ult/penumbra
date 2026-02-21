export module penumbra.editor:inspector;

import :camera_component;
import :light_components;
import :render_object_component;
import :widget;
import :world_state;

import penumbra.ecs;
import penumbra.resource;
import imgui;
import std;

using std::uint32_t;

namespace penumbra
{

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
				inspect_material(resource_manager_get_material(robj->material));
		}
	}
private:
	void inspect_transform(Transform& transform)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
		if(ImGui::CollapsingHeader("Transform", flags))
		{
			bool dirty = false;
			if(ImGui::DragFloat3("Translation", &transform.translation.x, 0.1f))
				dirty = true;

			if(ImGui::DragFloat4("Rotation", &transform.rotation.x, 0.01f, -1.0f, 1.0f))
				dirty = true;

			if(ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f))
				dirty = true;
		}
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

		}
	}

	void inspect_directional_light(directional_light_component& light)
	{
		if(ImGui::CollapsingHeader("Directional light"))
		{
			ImGui::DragFloat3("Direction", &light.direction.x, 0.01f, -1.0f, 1.0f);
			ImGui::ColorEdit3("Color", &light.color.x);
			ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 200000.0f, "%.0flx");
		}
	}

	void inspect_material(const MaterialResource& mtl)
	{
		if(ImGui::CollapsingHeader("Material"))
		{
			vec3 df = mtl.factors.diffuse;
			float rf = mtl.factors.roughness;
			float mf = mtl.factors.metallic;
			float nf = mtl.factors.normal;
			float refl = mtl.factors.reflectivity;
			vec3 ef = mtl.factors.emissive;
		
			ImGui::DragFloat3("Diffuse factor", &df.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Roughness factor", &rf, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Metallic factor", &mf, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Normal factor", &nf, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Reflectivity", &refl, 0.01f, 0.0f, 1.0f);
			ImGui::ColorEdit3("Emissive color", &ef.x);
		}
	}

	WorldState* world{nullptr};
};

}

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
		
		auto& transform = graph.get<Transform>(world->selected_entity);
		auto* camera = graph.try_get<camera_component>(world->selected_entity);
		auto* robj = graph.try_get<render_object_component>(world->selected_entity);
		auto* dlight = graph.try_get<directional_light_component>(world->selected_entity);
		auto* pl = graph.try_get<point_light_component>(world->selected_entity);
		auto* sl = graph.try_get<spotlight_component>(world->selected_entity);
		auto* anim = graph.try_get<animation_component>(world->selected_entity);
		auto* emitter = graph.try_get<emitter_component>(world->selected_entity);

		if(ImGui::Button("Add component"))
			ImGui::OpenPopup("component_add");

		if(ImGui::BeginPopupContextItem("component_add"))
		{
			if(!pl && ImGui::Selectable("Point light"))
			{
				pl = &graph.emplace<point_light_component>(world->selected_entity);
				pl->offset = renderer_write_point_light(RenderLightData
				{
					vec4{transform.translation, 0.0f},
					vec4{pl->color * pl->intensity, pl->radius},
					vec4{0.0f}
				});
			}

			if(!sl && ImGui::Selectable("Spotlight"))
			{
				sl = &graph.emplace<spotlight_component>(world->selected_entity);
				sl->offset = renderer_write_spot_light(RenderLightData
				{
					vec4{transform.translation, sl->inner_cone},
					vec4{sl->color * sl->intensity, sl->range},
					vec4{sl->direction, sl->outer_cone}
				});
			}

			if(!anim && ImGui::Selectable("Animation"))
			{
				anim = &graph.emplace<animation_component>(world->selected_entity, ResourceID{});
			}

			if(!emitter && ImGui::Selectable("Emitter"))
			{
				emitter = &graph.emplace<emitter_component>(world->selected_entity, renderer_create_particlesystem(1024));
			}	

			ImGui::EndPopup();
		}

		auto transform_dirty = inspect_transform(transform);
		if(transform_dirty)
			graph.emplace<transform_dirty_t>(world->selected_entity);

		if(camera)
			inspect_camera(*camera);

		if(robj)
		{
			if(robj->material.get_handle())
				inspect_material(robj->material);
		}
		
		if(dlight)
			inspect_directional_light(*dlight);

		if(pl)
		{
			auto dirty = inspect_point_light(*pl);
			if(dirty)
			{
				renderer_write_point_light(pl->offset, RenderLightData
				{
					vec4{transform.translation, 0.0f},
					vec4{pl->color * pl->intensity, pl->radius},
					vec4{0.0f}
				});
			}
		}

		if(sl)
		{
			auto dirty = inspect_spot_light(*sl);
			if(dirty)
			{
				renderer_write_spot_light(sl->offset, RenderLightData
				{
					vec4{transform.translation, sl->inner_cone},
					vec4{sl->color * sl->intensity, sl->range},
					vec4{sl->direction, sl->outer_cone}
				});
			}
		}

		if(anim)
		{
			inspect_animation(*anim);
		}
	}
private:
	bool inspect_transform(Transform& transform)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		bool dirty = false;
		if(ImGui::CollapsingHeader("Transform", flags))
		{
			dirty |= inspect_vec3("Translation", transform.translation);
			dirty |= inspect_quat("Rotation", transform.rotation);
			dirty |= inspect_vec3("Scale", transform.scale);
		}
		ImGui::PopStyleVar();

		return dirty;
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

			dirty |= ImGui::ColorEdit4("Color factor", &mtl.factors.albedo.x);
			dirty |= ImGui::DragFloat("Roughness factor", &mtl.factors.roughness, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::DragFloat("Metallic factor", &mtl.factors.metallic, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::DragFloat("Normal factor", &mtl.factors.normal, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::DragFloat("Reflectivity", &mtl.factors.reflectivity, 0.01f, 0.0f, 1.0f);
			dirty |= ImGui::ColorEdit3("Emissive color", &mtl.factors.emissive.x);

			bool has_alpha_cutoff = false;

			if(mtl.flags & RENDER_MATERIAL_ALPHA_MASK)
			{
				has_alpha_cutoff = true;
				ImGui::Text("MATERIAL_FLAG_ALPHA_MASK");
			}
			else if(mtl.flags & RENDER_MATERIAL_ALPHA_BLEND)
			{
				ImGui::Text("MATERIAL_FLAG_ALPHA_BLEND");
			}

			if(has_alpha_cutoff)
			{
				dirty |= ImGui::DragFloat("Alpha threshold", &mtl.factors.alpha_cf, 0.01f, 0.0f, 1.0f);
			}


			if(dirty)
				resource_manager_sync_material(rid);
		}
	}

	WorldState* world{nullptr};
};

}

export module penumbra.editor:viewport;

import :widget;
import :world_state;
import :render_object_component;

import penumbra.core;
import penumbra.math;
import penumbra.gpu;
import penumbra.ui;
import std;
using std::uint32_t;

namespace penumbra
{

export class Viewport : public Widget
{
public:
	Viewport(Window* wnd, GPUTextureDescriptor* rt, WorldState* ws) : Widget("Viewport", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse), window{wnd}, render_target{rt}, world{ws}
       	{
		vb_picking_cs = gpu_create_compute_pipeline(load_shader("shaders/visbuffer_picking"));
		picking_buffer = gpu_allocate_memory(sizeof(uint32_t), GPU_MEMORY_READBACK, GPU_BUFFER_STORAGE);
		renderer_add_visbuffer_hook([this](GPUCommandBuffer& cmd, VisbufferInfo vb_data, uint32_t frame_index){object_picking(cmd, vb_data, frame_index);});
	}

	~Viewport() override
	{
		gpu_free_memory(picking_buffer);
		gpu_destroy_pipeline(vb_picking_cs);
	}

	void update_render_target(GPUTextureDescriptor* rt)
	{
		render_target = rt;
	}

	void update_camera(const mat4& v, const mat4& p)
	{
		view = v;
		proj = p;
	}

	void configure() override
	{
		ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	}

	void on_draw() override
	{
		auto g_root_x = ImGui::GetWindowPos().x + ImGui::GetCursorPosX();
		auto g_root_y = ImGui::GetWindowPos().y + ImGui::GetCursorPosY();
		ImGui::Image(ImTextureID(std::intptr_t(render_target)), ImVec2(size.x, size.y), ImVec2(0, 0), ImVec2(1, 1));
		
		auto& camera_transform = world->entities.get<Transform>(world->main_camera);
		if(ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			auto delta = window->get_mouse_delta();
			auto rot = camera_transform.rotation;

			if(ImGui::IsKeyDown(ImGuiKey_LeftShift))
			{
				vec3 front = vector_world_forward * Quaternion::make_mat3(rot);
				vec3 right = vec3::normalize(vec3::cross(front, vector_world_up)); 
				
				vec3 target_dir{0.0f};
				if(delta.x < 0.0)
					target_dir += right;
				else if(delta.x > 0.0)
					target_dir -= right;

				if(delta.y < 0.0)
					target_dir -= front;
				else if(delta.y > 0.0)
					target_dir += front;

				camera_transform.translation += (target_dir * 3.0f * ImGui::GetIO().DeltaTime);
			}
			else
			{
				Quaternion yaw = Quaternion::from_axis_angle(vector_world_up, to_radians(-delta.x * 0.2f));
				Quaternion pitch = Quaternion::from_axis_angle(vector_world_right, to_radians(-delta.y * 0.2f));
				Quaternion new_rot = yaw * rot * pitch;
				camera_transform.rotation = new_rot;
			}
		}

		transform_gizmo(g_root_x, g_root_y);

		picking_pos.x = std::min(uint32_t(ImGui::GetIO().MousePos.x - g_root_x), size.x);
		picking_pos.y = std::min(uint32_t(ImGui::GetIO().MousePos.y - g_root_y), size.y);
		uint32_t object_id = *reinterpret_cast<uint32_t*>(gpu_map_memory(picking_buffer));

		if(ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing())
		{
			if(object_id == 0)
			{
				world->selected_entity = world->env;
			}
			else
			{
				for(auto [entity, robj] : world->entities.view<render_object_component>().each())
				{
					if(robj.renderer_objectID == object_id)
						world->selected_entity = entity;
				}
			}	
		}

		ui::draw_device_overlay({uint32_t(g_root_x), uint32_t(g_root_y)});
		ImGui::PopStyleVar(2);
	}
private:
	void transform_gizmo(float root_x, float root_y)
	{
		if(world->selected_entity == ecs::null)
			return;

		auto& transform = world->entities.get<Transform>(world->selected_entity);
		mat4 obj_mtx = transform.as_matrix();

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(root_x, root_y, float(size.x), float(size.y));

		// ImGuizmo uses OpenGL NDC
		proj[1][1] *= -1.0f;

		int op = ImGuizmo::OPERATION::TRANSLATE;

		bool snap = false;
		float snapping = 0.5f;

		vec3 snap_values{snapping};

		mat4 delta;
		ImGuizmo::Manipulate(&view[0][0], &proj[0][0], static_cast<ImGuizmo::OPERATION>(op), ImGuizmo::LOCAL, &obj_mtx[0][0], &delta[0][0], snap ? &snap_values.x : nullptr);
		if(ImGuizmo::IsUsing())
		{
			auto [nt, nr, ns] = decompose(obj_mtx);
			transform.translation = nt;
		}
	}

	void object_picking(GPUCommandBuffer& cmd, VisbufferInfo vb_data, uint32_t frame_index)
	{
		gpu_set_pipeline(cmd, vb_picking_cs);

		struct ShaderData
		{
			GPUDevicePointer instances;
			GPUDevicePointer output;
			uvec2 picking_pos;
			uint32_t visbuffer_tex;
		} shader_data;
		shader_data.instances = vb_data.instances;
		shader_data.output = gpu_host_to_device_pointer(picking_buffer);
		shader_data.picking_pos = picking_pos;
		shader_data.visbuffer_tex = vb_data.texture->handle;

		gpu_dispatch(cmd, &shader_data, {1u, 1u, 1u});
	}

	Window* window{nullptr};
	GPUTextureDescriptor* render_target{nullptr};
	WorldState* world{nullptr};

	mat4 view;
	mat4 proj;

	GPUPipeline vb_picking_cs;
	GPUPointer picking_buffer;
	uvec2 picking_pos{0u, 0u};
};

}

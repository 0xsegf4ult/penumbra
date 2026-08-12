#include <widgets/viewport.hpp>
#include <widgets/widget.hpp>
#include <world/state.hpp>
#include <world/components/render.hpp>

#include <penumbra/input.hpp>
#include <penumbra/ecs.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/ui.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

Viewport::Viewport(GPUTextureDescriptor* rt, WorldState* ws) : Widget("Viewport", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse), render_target{rt}, world{ws}
{
	vb_picking_cs = gpu_create_compute_pipeline(load_shader("shaders/visbuffer_picking"));
	picking_buffer = gpu_allocate_memory(sizeof(uint32_t), GPU_MEMORY_READBACK, GPU_BUFFER_STORAGE);
	renderer_hook_visbuffer([this](GPUCommandBuffer& cmd, visbuffer_data vb_data, u32 frame_index){object_picking(cmd, vb_data, frame_index);});
}

Viewport::~Viewport()
{
	gpu_free_memory(picking_buffer);
	gpu_destroy_pipeline(vb_picking_cs);
}

void Viewport::update_render_target(GPUTextureDescriptor* rt)
{
	render_target = rt;
}

void Viewport::update_camera(const mat4& v, const mat4& p)
{
	view = v;
	proj = p;
}

void Viewport::configure() 
{
	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
}

void Viewport::set_tool(ViewportTool tool)
{
	auto last_tool = this->tool;
	this->tool = tool;

	if(tool == ViewportTool::Fly)
	{
		input_set_mouse_capture(true);
	} 
	else if(last_tool == ViewportTool::Fly)
	{
		input_set_mouse_capture(false);
	}
}

void Viewport::on_draw() 
{
	auto g_root_x = ImGui::GetWindowPos().x + ImGui::GetCursorPosX();
	auto g_root_y = ImGui::GetWindowPos().y + ImGui::GetCursorPosY();
	ImGui::Image(ImTextureID(intptr_t(render_target)), ImVec2(size.x, size.y), ImVec2(0, 0), ImVec2(1, 1));
	
	auto& camera_transform = world->entities.get<Transform>(world->main_camera);
	if(tool != ViewportTool::Fly && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
	{
		auto delta = input_get_mouse_delta();
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
	else if(tool == ViewportTool::Fly)
	{
		auto rot = camera_transform.rotation;
		auto delta = input_get_mouse_delta();

		Quaternion yaw = Quaternion::from_axis_angle(vector_world_up, to_radians(-delta.x * 0.2f));
		Quaternion pitch = Quaternion::from_axis_angle(vector_world_right, to_radians(-delta.y * 0.2f));
		Quaternion new_rot = yaw * rot * pitch;
		camera_transform.rotation = new_rot;

		vec3 target_dir {0.0f};
		vec3 front = vector_world_forward * Quaternion::make_mat3(new_rot);
		vec3 right = vec3::normalize(vec3::cross(front, vector_world_up));
		if(ImGui::IsKeyDown(ImGuiKey_W))
			target_dir += front;
		if(ImGui::IsKeyDown(ImGuiKey_S))
			target_dir -= front;
		if(ImGui::IsKeyDown(ImGuiKey_A))
			target_dir -= right;
		if(ImGui::IsKeyDown(ImGuiKey_D))
			target_dir += right;

		if(ImGui::IsKeyDown(ImGuiKey_Escape))
			set_tool(ViewportTool::Translate);

		camera_transform.translation += (target_dir * 3.0f * ImGui::GetIO().DeltaTime);
	}

	transform_gizmo(g_root_x, g_root_y);

	picking_pos.x = std::min(u32(ImGui::GetIO().MousePos.x - g_root_x), size.x);
	picking_pos.y = std::min(u32(ImGui::GetIO().MousePos.y - g_root_y), size.y);
	u32 object_id = *reinterpret_cast<u32*>(gpu_map_memory(picking_buffer));

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
				if((robj.renderer_objectID & 0xFFFFFF) == object_id)
					world->selected_entity = entity;
			}
		}	
	}

	ui::draw_device_overlay({u32(g_root_x), u32(g_root_y)});
	ImGui::PopStyleVar(2);
}

void Viewport::transform_gizmo(float root_x, float root_y)
{
	if(world->selected_entity == ecs::null)
		return;

	auto& transform = world->entities.get<Transform>(world->selected_entity);
	mat4 obj_mtx = get_entity_world_matrix(world->entities, world->selected_entity);

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(root_x, root_y, float(size.x), float(size.y));

	// ImGuizmo uses OpenGL NDC
	proj[1][1] *= -1.0f;

	int op;
	switch(tool)
	{
	case ViewportTool::Translate:
		op = ImGuizmo::OPERATION::TRANSLATE;
		break;
	case ViewportTool::Rotate:
		op = ImGuizmo::OPERATION::ROTATE;
		break;
	case ViewportTool::Scale:	
		op = ImGuizmo::OPERATION::SCALE;
		break;
	default:
		return;
	}

	bool snap = ImGui::IsKeyDown(ImGuiMod_Ctrl);
	float snapping = 0.5f;
	if(tool == ViewportTool::Rotate)
		snapping = 45.0f;

	vec3 snap_values{snapping};

	mat4 delta;
	ImGuizmo::Manipulate(&view[0][0], &proj[0][0], static_cast<ImGuizmo::OPERATION>(op), ImGuizmo::LOCAL, &obj_mtx[0][0], &delta[0][0], snap ? &snap_values.x : nullptr);
	if(ImGuizmo::IsUsing())
	{
		auto [nt, nr, ns] = decompose(delta);
		if(tool == ViewportTool::Translate)
			transform.translation += nt;
		else if(tool == ViewportTool::Rotate)
			transform.rotation = transform.rotation * nr;
		else if(tool == ViewportTool::Scale)
			transform.scale = ns;

		world->entities.emplace_or_replace<transform_dirty_t>(world->selected_entity);
	}
}

void Viewport::object_picking(GPUCommandBuffer& cmd, visbuffer_data vb_data, u32 frame_index)
{
	gpu_set_pipeline(cmd, vb_picking_cs);

	struct ShaderData
	{
		GPUDevicePointer instances;
		GPUDevicePointer output;
		uvec2 picking_pos;
		u32 visbuffer_tex;
	} shader_data;
	shader_data.instances = vb_data.instances;
	shader_data.output = gpu_host_to_device_pointer(picking_buffer);
	shader_data.picking_pos = picking_pos;
	shader_data.visbuffer_tex = vb_data.texture->handle;

	gpu_dispatch(cmd, &shader_data, {1u, 1u, 1u});
}

}

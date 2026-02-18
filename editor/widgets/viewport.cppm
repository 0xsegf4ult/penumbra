export module penumbra.editor:viewport;

import :widget;
import :world_state;

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
	Viewport(Window* wnd, GPUTextureDescriptor* rt, WorldState* ws) : Widget("Viewport", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse), window{wnd}, render_target{rt}, world{ws} {}
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

			Quaternion yaw = Quaternion::from_axis_angle(vector_world_up, to_radians(delta.x));
			Quaternion pitch = Quaternion::from_axis_angle(vector_world_right, to_radians(delta.y));
			Quaternion new_rot = yaw * rot * pitch;
			camera_transform.rotation = new_rot;
		}
		
		ui::draw_device_overlay({uint32_t(g_root_x), uint32_t(g_root_y)});
		ImGui::PopStyleVar(2);
	}
private:
	Window* window{nullptr};
	GPUTextureDescriptor* render_target{nullptr};
	WorldState* world{nullptr};
};

}

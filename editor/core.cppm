export module penumbra.editor;

import penumbra.core;
import penumbra.gpu;
import penumbra.renderer;
import penumbra.ui;

import imgui;
import std;

import :prefab;
import :camera_component;
import :inspector;
import :scenegraph;
import :widget;
import :viewport;
export import :world_state;

using std::uint32_t;

namespace penumbra
{

export class Editor
{
public:
	Editor(Window& wnd, WorldState* ws, int argc, const char** argv) : window{wnd}, world{ws}
	{
		imgui_add_hook([this](){draw_ui();});

		widgets.push_back(std::make_unique<Viewport>(&wnd, renderer_get_framebuffer(), world));
		widget_viewport = widgets.back().get();

		widgets.push_back(std::make_unique<ScenegraphView>(world));
		widgets.push_back(std::make_unique<Inspector>(world));


		load_prefab(*world, "bistrov2");
	}

	~Editor()
	{
	}

	void fixed_update(double dt)
	{

	}
		
	void variable_update(double dt)
	{
		auto vp_size = widget_viewport->get_size();
		if(vp_size.x && vp_size.y)
			renderer_update_render_resolution(vp_size);	
		update_main_camera();
	}

	void draw_ui()
	{
		auto window_flags = 
			ImGuiWindowFlags_MenuBar		| 
			ImGuiWindowFlags_NoDocking		| 
			ImGuiWindowFlags_NoTitleBar		| 
			ImGuiWindowFlags_NoCollapse		|
				ImGuiWindowFlags_NoResize		|
			ImGuiWindowFlags_NoMove			|
			ImGuiWindowFlags_NoBringToFrontOnFocus	|
			ImGuiWindowFlags_NoNavFocus;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		bool open = true;
		ImGui::Begin("Dockspace", &open, window_flags);
		ImGui::PopStyleVar(3);
		ImGuiID dockspace_id = ImGui::GetID("Dockspace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		menubar_draw();
		for(auto& widget : widgets)
			widget->draw();

		ImGui::End();
	}
private:
	void update_main_camera()
	{
		auto& camera_transform = world->entities.get<Transform>(world->main_camera);
		auto& camera = world->entities.get<camera_component>(world->main_camera);
		auto matrix = camera_transform.as_matrix();
		auto res = renderer_get_render_resolution();

		const float near = camera.near_plane;

		mat4 view = mat4::make_translation(-camera_transform.translation);

		mat4 proj;

		const float focal_length = 1.0f / std::tan(to_radians(camera.vertical_fov) / 2.0f);
		const float aspect_ratio = static_cast<float>(res.w) / static_cast<float>(res.h);
		const float x = focal_length / aspect_ratio;
		const float y = -focal_length;

		proj =
		{
			vec4{x,	    0.0f,  0.0f,  0.0f},
			vec4{0.0f,     y,  0.0f,  0.0f},
			vec4{0.0f,  0.0f,  0.0f, -1.0f},
			vec4{0.0f,  0.0f,  near,  0.0f}
		};


		renderer_update_camera(view, proj, camera.get_exposure());
	}


	void menubar_draw()
	{
		if(ImGui::BeginMenuBar())
		{
			if(ImGui::BeginMenu("Tools"))
			{
				ImGui::EndMenu();
			}

			if(ImGui::BeginMenu("Settings"))
			{
				if(ImGui::BeginMenu("Display"))
				{
					if(ImGui::BeginMenu("Present mode"))
					{
						static GPUPresentMode pmode = GPU_PRESENT_MODE_IMMEDIATE; 
						if(ImGui::MenuItem("FIFO", nullptr, pmode == GPU_PRESENT_MODE_FIFO))
						{
							pmode = GPU_PRESENT_MODE_FIFO;
							gpu_swapchain_set_present_mode(pmode);
						}

						if(ImGui::MenuItem("FIFO_RELAXED", nullptr, pmode == GPU_PRESENT_MODE_FIFO_RELAXED))
						{
							pmode = GPU_PRESENT_MODE_FIFO_RELAXED;
							gpu_swapchain_set_present_mode(pmode);
						}

						if(ImGui::MenuItem("IMMEDIATE", nullptr, pmode == GPU_PRESENT_MODE_IMMEDIATE))
						{
							pmode = GPU_PRESENT_MODE_IMMEDIATE;
							gpu_swapchain_set_present_mode(pmode);
						}

						ImGui::EndMenu();
					}

					if(ImGui::BeginMenu("Window mode"))
					{
						auto is_fullscreen = window.is_fullscreen();
						if(ImGui::MenuItem("Windowed", nullptr, !is_fullscreen))
						{
							window.set_fullscreen(false);
						}

						if(ImGui::MenuItem("Borderless fullscreen", nullptr, is_fullscreen))
						{
							window.set_fullscreen(true);
						}

						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}
	}

	Window& window;
	WorldState* world;
	std::vector<std::unique_ptr<Widget>> widgets;
	Widget* widget_viewport;
};

}

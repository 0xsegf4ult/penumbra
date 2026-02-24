export module penumbra.editor;

import penumbra.core;
import penumbra.gpu;
import penumbra.renderer;
import penumbra.ui;

import imgui;
import std;

import :envmap;
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

		create_rendertarget();
		widgets.push_back(std::make_unique<Viewport>(&wnd, &framebuffer, world));
		widget_viewport = reinterpret_cast<Viewport*>(widgets.back().get());

		widgets.push_back(std::make_unique<ScenegraphView>(world));
		widgets.push_back(std::make_unique<Inspector>(world));

		auto envmap = load_envmap("hdri/kloppenheim");
		renderer_set_envmap
		(RenderEnvironmentMap{
			.irradiance = resource_manager_get_texture(envmap.irradiance).descriptor,
			.prefiltered = resource_manager_get_texture(envmap.prefiltered).descriptor
		});		

		vfs::path def_prefab = "bistrov2";
		if(argc >= 2)
			def_prefab = argv[1];

		load_prefab(*world, def_prefab);
	}

	~Editor()
	{
		gpu_destroy_texture(framebuffer_tex);
	}

	void fixed_update(double dt)
	{

	}
		
	void variable_update(double dt)
	{
		if(!sim_running)
		{
			auto vp_size = widget_viewport->get_size();
			if(vp_size.x && vp_size.y)
			{
				renderer_update_render_resolution(vp_size);

				if(last_vp_size != vp_size)
				{
					last_vp_size = vp_size;

					gpu_wait_idle();
					gpu_free_descriptor(framebuffer);
					gpu_destroy_texture(framebuffer_tex);

					create_rendertarget();
				}
			}

			widget_viewport->update_render_target(&framebuffer);
			renderer_set_output_rendertarget(framebuffer_tex);
		}
		else
		{
			if(window.is_key_down(SCANCODE_ESCAPE))
			{
				sim_running = false;
				window.set_capture_mouse(false);
			}

			auto wdim = window.get_size();
			renderer_update_render_resolution(wdim);

			auto& camera_transform = world->entities.get<Transform>(world->main_camera);
			auto rot = camera_transform.rotation;

			auto delta = window.get_mouse_delta();

			Quaternion yaw = Quaternion::from_axis_angle(vector_world_up, to_radians(-delta.x * 0.2f));
			Quaternion pitch = Quaternion::from_axis_angle(vector_world_right, to_radians(-delta.y * 0.2f));
			Quaternion new_rot = yaw * rot * pitch;
			camera_transform.rotation = new_rot;

			vec3 target_dir{0.0f};
			vec3 front = vector_world_forward * Quaternion::make_mat3(new_rot);
			vec3 right = vec3::normalize(vec3::cross(front, vector_world_up));
			if(window.is_key_down(SCANCODE_W))
				target_dir += front;
			if(window.is_key_down(SCANCODE_S))
				target_dir -= front;
			if(window.is_key_down(SCANCODE_A))
				target_dir -= right;
			if(window.is_key_down(SCANCODE_D))
				target_dir += right;

			camera_transform.translation += (target_dir * 3.0f * dt);
		}

		update_main_camera();
		update_env();
	}

	void draw_ui()
	{
		if(sim_running)
		{
			ui::draw_device_overlay();
			return;
		}

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
		renderer_imgui_panel();
		for(auto& widget : widgets)
			widget->draw();

		ImGui::End();
	}
private:
	void create_rendertarget()
	{
		framebuffer_tex = gpu_create_texture
		({
			.dim = uvec3{last_vp_size, 1u},
			.format = GPU_FORMAT_RGBA8_SRGB,
		       	.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_COLOR_ATTACHMENT	
		});
		framebuffer = gpu_texture_view_descriptor(framebuffer_tex, {.format = GPU_FORMAT_RGBA8_SRGB});

		auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
		gpu_texture_layout_transition(cmd, framebuffer_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
		gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	}

	void update_main_camera()
	{
		auto& camera_transform = world->entities.get<Transform>(world->main_camera);
		auto& camera = world->entities.get<camera_component>(world->main_camera);
		auto res = renderer_get_render_resolution();

		const float near = camera.near_plane;

		const float focal_length = 1.0f / std::tan(to_radians(camera.vertical_fov) / 2.0f);
		const float aspect_ratio = static_cast<float>(res.w) / static_cast<float>(res.h);
		const float x = focal_length / aspect_ratio;
		const float y = -focal_length;

		RenderCameraData cam_data
		{
			.view = mat4::make_translation(-camera_transform.translation) * Quaternion::make_mat4(~camera_transform.rotation),
			.proj = 
			{
				vec4{x,	    0.0f,  0.0f,  0.0f},
				vec4{0.0f,     y,  0.0f,  0.0f},
				vec4{0.0f,  0.0f,  0.0f, -1.0f},
				vec4{0.0f,  0.0f,  near,  0.0f}
			},
			.position = camera_transform.translation,
			.znear = near,
			.zfar = camera.far_plane,
			.exposure = camera.get_exposure()
		};

		renderer_update_camera(cam_data);
		widget_viewport->update_camera(cam_data.view, cam_data.proj);
	}

	void update_env()
	{
		auto dlight = world->entities.get<directional_light_component>(world->env);
		renderer_update_environment
		({
			dlight.direction,
			dlight.color,
			dlight.intensity
		});
	}

	void menubar_draw()
	{
		if(ImGui::BeginMenuBar())
		{
			if(ImGui::Button("Run"))
			{
				sim_running = true;
				auto wdim = window.get_size();
				renderer_update_render_resolution(wdim);
				renderer_set_output_rendertarget(GPUTexture{0});
				window.set_capture_mouse(true);
			}

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
	Viewport* widget_viewport;

	uvec2 last_vp_size{800u, 600u};
	GPUTexture framebuffer_tex;
	GPUTextureDescriptor framebuffer;

	bool sim_running = false;
};

}

export module penumbra.editor;

import penumbra.core;
import penumbra.gpu;
import penumbra.renderer;
import penumbra.ui;
import penumbra.physics;

import imgui;
import std;

import :envmap;
import :prefab;
import :camera_component;
import :physics_components;
import :inspector;
import :resource_window;
import :scenegraph;
import :widget;
import :viewport;
export import :world_state;
import :import_gltf;

using std::uint32_t, std::size_t;

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

		widgets.push_back(std::make_unique<ResourceWindow>());
		widgets.push_back(std::make_unique<ScenegraphView>(world));
		widgets.push_back(std::make_unique<Inspector>(world));

		auto envmap = load_envmap("hdri/kloppenheim");
		//auto envmap = load_envmap("hdri/kloppenheim_night");
		renderer_set_envmap
		(RenderEnvironmentMap{
			.irradiance = resource_manager_get_texture(envmap.irradiance).descriptor,
			.prefiltered = resource_manager_get_texture(envmap.prefiltered).descriptor
		});		

		vfs::path def_prefab = "bistrov2";
		if(argc >= 2)
			def_prefab = argv[1];

		load_prefab(*world, def_prefab);

		if(def_prefab == "qmv_arena")
		{
			ecs::entity floor_ent;
			for(auto&& [entity, name]: world->entities.view<entity_name>().each())
			{
				if(name == "qmv_floor")
				{
					floor_ent = entity;	
					break;
				}
			}
			auto floor_xform = world->entities.get<Transform>(floor_ent);

			auto floor_shape = physics_create_box({.edges = vec3{50.0f, 0.1f, 50.0f}});
			auto floor_rb = physics_create_body(world->phys, 
			{
				.initial_transform = floor_xform,
				.shape = floor_shape
			});
			pdbg_ec_target = floor_ent;

			world->entities.emplace<rigidbody_component>(floor_ent, floor_rb);

			auto test_entity = world->spawn("test_entity");
			pdbg_ec_source = test_entity;
			add_entity_as_child(world->entities, world->root, test_entity);
			Transform te_xf{vec3{0.0f, 5.5f, 0.0f}, Quaternion{}, vec3{1.0f}};
			world->entities.replace<Transform>(test_entity, te_xf);

			ResourceID def_mat;
			auto capsule = resource_manager_load_geometry("meshes/capsule");  
			auto& caps_data = resource_manager_get_geometry(capsule);
		
			auto rd_object = renderer_world_insert_object
			({
				world->entities.get<Transform>(test_entity).as_matrix(),
				RENDER_BUCKET_DEFAULT,
				caps_data.sphere,
				def_mat.get_handle(),
				caps_data.l0_cluster_count,
				caps_data.lod_offset,
				caps_data.lod_count,
				caps_data.vertex_offset,
				caps_data.index_offset,
				caps_data.cluster_offset
			}, 4);
			world->entities.emplace<render_object_component>(test_entity, capsule, def_mat, rd_object);
			auto caps_shape = physics_create_capsule({.radius = 0.5f, .height = 1.0f});
			auto caps_rb = physics_create_body(world->phys, 
			{
				.initial_transform = te_xf,
				.shape = caps_shape
			});
			world->entities.emplace<rigidbody_component>(test_entity, caps_rb);

			auto box = world->spawn("box");
			pdbg_ec_target = box;
			add_entity_as_child(world->entities, world->root, box);
			Transform bo_xf{vec3{0.0f, 1.5f, 0.0f}, Quaternion{}, vec3{1.0f}};
			world->entities.replace<Transform>(box, bo_xf);

			auto boxg = resource_manager_load_geometry("meshes/unit_cube");
			auto& box_data = resource_manager_get_geometry(boxg);

			auto b_rd_object = renderer_world_insert_object
			({
				bo_xf.as_matrix(),
				RENDER_BUCKET_DEFAULT,
				box_data.sphere,
				def_mat.get_handle(),
				box_data.l0_cluster_count,
				box_data.lod_offset,
				box_data.lod_count,
				box_data.vertex_offset,
				box_data.index_offset,
				box_data.cluster_offset
			}, 4);
			world->entities.emplace<render_object_component>(box, boxg, def_mat, b_rd_object);
			auto box_shape = physics_create_box({.edges=vec3{1.0f, 1.0f, 1.0f}});
			auto box_rb = physics_create_body(world->phys,
			{
				.initial_transform = bo_xf,
				.shape = box_shape
			});
			world->entities.emplace<rigidbody_component>(box, box_rb);
		}
	}

	~Editor()
	{
		gpu_destroy_texture(framebuffer_tex);
	}

	void fixed_update(double dt)
	{
		for(auto [entity, transform, rb] : world->entities.view<Transform, rigidbody_component>().each())
			physics_body_set_transform(rb.handle, transform);

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

		update_transforms();
		update_main_camera();
		update_env();

		for(auto [entity, anim] : world->entities.view<animation_component>().each())
		{
			if(!anim.animation.get_handle())
				continue;

			Animation& res = resource_manager_get_animation(anim.animation);

			if(anim.running)
				anim.cur_time += dt;

			if(anim.cur_time > res.end_time)
			{
				if(!anim.loop)
					anim.running = false;

				anim.cur_time -= res.end_time;
			}
		}

		for(auto [entity, transform, r_anim] : world->entities.view<Transform, render_anim_component>().each())
		{
			if(!r_anim.sg_instance || !r_anim.skeleton.get_handle())
				continue;
		
			auto& skeleton = resource_manager_get_skeleton(r_anim.skeleton);
			tmp_bone_ls.resize(std::max(tmp_bone_ls.size(), size_t(skeleton.bone_count)));
			tmp_bone_ws.resize(std::max(tmp_bone_ws.size(), size_t(skeleton.bone_count)));

			for(uint32_t i = 0; i < skeleton.bone_count; i++)
				tmp_bone_ls[i] = skeleton.bone_transforms[i];

			auto* anim_c = world->entities.try_get<animation_component>(entity);
			if(!anim_c)
				anim_c = world->entities.try_get<animation_component>(world->entities.get<entity_relationship>(entity).parent);

			if(anim_c && anim_c->animation.get_handle())
			{
				Animation& anim = resource_manager_get_animation(anim_c->animation);

				for(auto& channel : anim.channels)
				{
					for(auto i = 0zu; i < channel.timestamps.size() - 1; i++)
					{
						if((anim_c->cur_time >= channel.timestamps[i]) && (anim_c->cur_time <= channel.timestamps[i + 1]))
						{
							float a = (anim_c->cur_time - channel.timestamps[i]) / (channel.timestamps[i + 1] / channel.timestamps[i]);
							switch(channel.path)
							{
							case AnimationPath::Translation:
								tmp_bone_ls[channel.bone].translation = mix(channel.values_as_vec3()[i], channel.values_as_vec3()[i + 1], a);
								break;
							case AnimationPath::Rotation:
								tmp_bone_ls[channel.bone].rotation = Quaternion::normalize(Quaternion::slerp(channel.values_as_quat()[i], channel.values_as_quat()[i + 1], a));
								break;
							case AnimationPath::Scale:
								tmp_bone_ls[channel.bone].scale = mix(channel.values_as_vec3()[i], channel.values_as_vec3()[i + 1], a);
								break;
							}
						}
					}
				}
				
			}
				
			for(uint32_t i = 0; i < skeleton.bone_count; i++)
			{
				if(skeleton.bone_parents[i] > 0)
					tmp_bone_ws[i] = tmp_bone_ls[i].as_matrix() * tmp_bone_ws[skeleton.bone_parents[i] - 1];
				else
					tmp_bone_ws[i] = tmp_bone_ls[i].as_matrix();
			}

			for(uint32_t i = 0; i < skeleton.bone_count; i++)
				tmp_bone_ws[i] = skeleton.bone_inv_bind_matrices[i] * tmp_bone_ws[i];

			renderer_write_anim_bones(r_anim.sg_instance, tmp_bone_ws.data(), skeleton.bone_count);
		}
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

		physics_debug();
	}
private:
	void create_rendertarget()
	{
		framebuffer_tex = gpu_create_texture
		({
			.dim = uvec3{last_vp_size, 1u},
			.format = GPU_FORMAT_BGRA8_SRGB,
		       	.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_COLOR_ATTACHMENT	
		});
		framebuffer = gpu_texture_view_descriptor(framebuffer_tex, {.format = GPU_FORMAT_BGRA8_SRGB});

		auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
		gpu_texture_layout_transition(cmd, framebuffer_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
		gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	}

	void update_entity_subtree(ecs::entity entity, mat4 matrix_world)
	{
		auto* robj = world->entities.try_get<render_object_component>(entity);
		if(robj)
		{
			renderer_world_update_object(robj->renderer_objectID, matrix_world);
		}
		
		entity_relationship& re = world->entities.get<entity_relationship>(entity);
		ecs::entity child = re.first_child;
		while(world->entities.valid(child))
		{
			update_entity_subtree(child, world->entities.get<Transform>(child).as_matrix() * matrix_world);
			child = world->entities.get<entity_relationship>(child).next_sibling;
		}
	}

	void update_transforms()
	{
		for(auto [entity] : world->entities.view<transform_dirty_t>().each())
		{
			auto wm = get_entity_world_matrix(world->entities, entity);
			
			update_entity_subtree(entity, wm);

			world->entities.remove<transform_dirty_t>(entity);
		}
	}

	void update_main_camera()
	{
		auto& camera_transform = world->entities.get<Transform>(world->main_camera);
		auto& camera = world->entities.get<camera_component>(world->main_camera);
		auto res = renderer_get_render_resolution();

		/*camera.aperture = 1.4f;
		camera.shutter_speed = 2.4f;
*/
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
			dlight.intensity,
			1200.0f
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

			if(ImGui::BeginMenu("File"))
			{
				if(ImGui::BeginMenu("Import"))
				{
					if(ImGui::MenuItem("GLTF"))
					{
						import_gltf_open = true;
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}

			if(ImGui::BeginMenu("Tools"))
			{
				auto tool = widget_viewport->get_tool();
				if(ImGui::MenuItem("Grab", "G", tool == ViewportTool::Translate))
					widget_viewport->set_tool(ViewportTool::Translate);
				if(ImGui::MenuItem("Rotate", "R", tool == ViewportTool::Rotate))
					widget_viewport->set_tool(ViewportTool::Rotate);
				if(ImGui::MenuItem("Scale", "S", tool == ViewportTool::Scale))
					widget_viewport->set_tool(ViewportTool::Scale);

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

		if(import_gltf_open)
			ImGui::OpenPopup("Import GLTF##modal");

		if(ImGui::BeginPopupModal("Import GLTF##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static std::string gltf_path = "";
			ImGui::InputText("Path", &gltf_path);
			if(ImGui::Button("Import"))
			{
				gltf_import_context ctx{.world = world};
				import_gltf(ctx, gltf_path);
				import_gltf_open = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if(ImGui::Button("Cancel"))
			{
				import_gltf_open = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void physics_debug()
	{
		bool has_rc = true;

		
		ImGui::Begin("Physics Debug");
		static char ec_name[128] = "";
		ImGui::InputTextWithHint("Source entity", "no source", ec_name, 128);
		if(ImGui::BeginDragDropTarget())
		{
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_and_drop_ecs_entity"))
			{
				pdbg_ec_source= *static_cast<ecs::entity*>(payload->Data);
				if(pdbg_ec_source != ecs::null)
				{
					auto& nname = world->entities.get<entity_name>(pdbg_ec_source);
					std::strncpy(ec_name, nname.c_str(), std::min(127zu, nname.length()));
					ec_name[nname.length()] = '\0';
				}
			}
			ImGui::EndDragDropTarget();
		}
		if(pdbg_ec_source != ecs::null && !world->entities.try_get<rigidbody_component>(pdbg_ec_source))
		{
			has_rc = false;
			ImGui::TextColored(ImColor(1.0f, 1.0f, 0.0f, 1.0f), "Entity does not have Rigidbody component!");
		}

		static char ect_name[128] = "";
		ImGui::InputTextWithHint("Target entity", "no target", ect_name, 128);
		if(ImGui::BeginDragDropTarget())
		{
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_and_drop_ecs_entity"))
			{
				pdbg_ec_target = *static_cast<ecs::entity*>(payload->Data);
				if(pdbg_ec_target != ecs::null)
				{
					auto& nname = world->entities.get<entity_name>(pdbg_ec_target);
					std::strncpy(ect_name, nname.c_str(), std::min(127zu, nname.length()));
					ect_name[nname.length()] = '\0';
				}
			}
			ImGui::EndDragDropTarget();
		}
		if(pdbg_ec_target != ecs::null && !world->entities.try_get<rigidbody_component>(pdbg_ec_target))
		{
			has_rc = false;
			ImGui::TextColored(ImColor(1.0f, 1.0f, 0.0f, 1.0f), "Entity does not have Rigidbody component!");
		}

		ImGui::Combo("psim mode", &psim_mode, "gjk_distance\0gjk_cast\0\0");

		if(pdbg_ec_source != ecs::null && pdbg_ec_target != ecs::null && has_rc)
		{
			if(psim_mode == 0)
			{
			
			auto body1 = world->entities.get<rigidbody_component>(pdbg_ec_source).handle;
			auto body2 = world->entities.get<rigidbody_component>(pdbg_ec_target).handle;

			auto transform_a = physics_body_get_transform(body1);

			physicsDistanceInput gcfg
			{
				.shape_a = physics_body_get_shape(body1),
				.shape_b = physics_body_get_shape(body2),
				.transform_a = transform_a,
				.transform_b = physics_body_get_transform(body2)
			};
			
			auto res = physics_shape_distance(gcfg);

			vec3 p_a = (vec4{res.point_a, 1.0f} * transform_a.as_matrix()).demote<3>();
			vec3 p_b = (vec4{res.point_b, 1.0f} * transform_a.as_matrix()).demote<3>();

			int cstatus = 2;
			float sum_cvr = gcfg.shape_a.get_convex_radius() + gcfg.shape_b.get_convex_radius();
			auto dst = res.distance;

			if(dst > sum_cvr)
			{
				cstatus = 0;
				ImGui::Text("Not colliding: dist %.6f", dst - sum_cvr);
			}
			else if(dst > 0.0f)
			{
				cstatus = 1;
				ImGui::Text("Collision: dist %.6f", dst - sum_cvr);
			}
			else
				ImGui::Text("Collision indeterminate: dist %.6f", dst - sum_cvr);
			renderer_debug_line(p_a + (res.normal * gcfg.shape_a.get_convex_radius() / dst), p_b - (res.separating_axis * gcfg.shape_b.get_convex_radius() / dst), vec3{cstatus == 1 ? 1.0f : 0.0f, cstatus == 0 ? 1.0f : 0.0f, cstatus == 2 ? 1.0f : 0.0f});

			renderer_debug_line(p_a, p_a + res.normal * dst, vec3{1.0f, 0.0f, 0.0f});

			}
			else if(psim_mode == 1)
			{
			
			auto mover = world->entities.get<rigidbody_component>(pdbg_ec_source).handle;
			auto target = world->entities.get<rigidbody_component>(pdbg_ec_target).handle;

			static vec3 gjk_cast_dir{0.0f, -2.0f, 0.0f};

			auto cres = physics_shape_cast
			({
				physics_body_get_shape(target),
				physics_body_get_shape(mover),
				physics_body_get_transform(target),
				physics_body_get_transform(mover),
				gjk_cast_dir
			});

			if(cres.hit)
			{
				ImGui::Text("Fraction %.6f", cres.fraction);
			}
			else
				ImGui::Text("Cast did not hit");

			}
		}

		ImGui::End();
	}

	Window& window;
	WorldState* world;
	std::vector<std::unique_ptr<Widget>> widgets;
	Viewport* widget_viewport;

	uvec2 last_vp_size{800u, 600u};
	GPUTexture framebuffer_tex;
	GPUTextureDescriptor framebuffer;

	bool sim_running = false;

	std::vector<Transform> tmp_bone_ls;	
	std::vector<mat4> tmp_bone_ws;

	bool import_gltf_open = false;

	ecs::entity pdbg_ec_source{ecs::null};
	ecs::entity pdbg_ec_target{ecs::null};
	int psim_mode{0};
};

}

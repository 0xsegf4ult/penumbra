#include <core.hpp>
#include <import/qmap.hpp>
#include <import/gltf.hpp>
#include <world/state.hpp>
#include <world/envmap.hpp>
#include <world/prefab.hpp>
#include <world/components/animation.hpp>
#include <world/components/camera.hpp>
#include <world/components/lights.hpp>
#include <world/components/render.hpp>
#include <world/components/physics.hpp>
#include <widgets/widget.hpp>
#include <widgets/console.hpp>
#include <widgets/inspector.hpp>
#include <widgets/viewport.hpp>
#include <widgets/scenegraph.hpp>

#include <penumbra/cvar.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/physics.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/window.hpp>
#include <penumbra/ui.hpp>

#include <memory>

namespace penumbra
{

Editor::Editor(window_t wnd, WorldState* ws, int argc, const char** argv) : window{wnd}, world{ws}
{
	imgui_add_hook([this](){draw_ui();});

	create_rendertarget();
	widgets.push_back(std::make_unique<Viewport>(&framebuffer, world));
	widget_viewport = reinterpret_cast<Viewport*>(widgets.back().get());

	widgets.push_back(std::make_unique<ScenegraphView>(world));
	widgets.push_back(std::make_unique<Inspector>(world));
	widgets.push_back(std::make_unique<ConsoleWidget>());
	console = reinterpret_cast<ConsoleWidget*>(widgets.back().get());	

	vfs_path def_prefab = "bistrov2";
	if(argc >= 2)
		def_prefab = argv[1];
/*
	qmap_import_context import_ctx{world};
	import_qmap(import_ctx, def_prefab);*/
	load_prefab(*world, def_prefab);
	try_patch_prefab(def_prefab.string());
}

Editor::~Editor()
{
	gpu_destroy_texture(framebuffer_tex);
}

void Editor::fixed_update(double dt)
{
}

void Editor::variable_update(double dt)
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

	world->update_transforms();
	update_main_camera();
	update_env();

	for(auto [entity, anim] : world->entities.view<animation_component>().each())
	{
		if(!resource_get_handle(anim.animation))
			continue;

		animation_resource& res = resource_manager_get_animation(anim.animation);

		if(anim.running)
			anim.cur_time += dt;

		if(anim.cur_time > res.end_time)
		{
			if(!anim.loop)
				anim.running = false;

			anim.cur_time -= res.end_time;
		}
	}

	Transform tmp_bone_ls[64];
	mat4 tmp_bone_ws[64];

	for(auto [entity, transform, r_obj, r_skel] : world->entities.view<Transform, render_object_component, render_skeleton_component>().each())
	{
		if(!resource_get_handle(r_skel.skeleton))
			continue;

		auto& skeleton = resource_manager_get_skeleton(r_skel.skeleton);
		assert(skeleton.bone_count < 64);
		
		for(u32 i = 0; i < skeleton.bone_count; i++)
			tmp_bone_ls[i] = skeleton.bone_transforms[i];

		auto* anim_c = world->entities.try_get<animation_component>(entity);
		if(!anim_c)
			anim_c = world->entities.try_get<animation_component>(world->entities.get<entity_relationship>(entity).parent);

		if(anim_c && resource_get_handle(anim_c->animation))
		{
			animation_resource& anim = resource_manager_get_animation(anim_c->animation);
			
			for(auto& channel : anim.channels)
			{
				for(u32 i = 0u; i < channel.timestamps.size() - 1; i++)
				{
					if((anim_c->cur_time >= channel.timestamps[i]) && (anim_c->cur_time <= channel.timestamps[i + 1]))
					{
						float a = (anim_c->cur_time - channel.timestamps[i]) / (channel.timestamps[i + 1] / channel.timestamps[i]);
						switch(channel.path)
						{
						case ANIM_PATH_TRANSLATION:
							tmp_bone_ls[channel.bone].translation = mix(reinterpret_cast<vec3*>(channel.values.data())[i], reinterpret_cast<vec3*>(channel.values.data())[i + 1], a);
							break;
						case ANIM_PATH_ROTATION:
							tmp_bone_ls[channel.bone].rotation = Quaternion::normalize(Quaternion::slerp(reinterpret_cast<Quaternion*>(channel.values.data())[i], reinterpret_cast<Quaternion*>(channel.values.data())[i + 1], a));
							break;
						case ANIM_PATH_SCALE:
							tmp_bone_ls[channel.bone].scale = mix(reinterpret_cast<vec3*>(channel.values.data())[i], reinterpret_cast<vec3*>(channel.values.data())[i + 1], a);
							break;
						}
					}
				}
			}
		}

		for(u32 i = 0; i < skeleton.bone_count; i++)
		{
			if(skeleton.bone_parents[i] > 0)
				tmp_bone_ws[i] = tmp_bone_ls[i].as_matrix() * tmp_bone_ws[skeleton.bone_parents[i] - 1];
			else
				tmp_bone_ws[i] = tmp_bone_ls[i].as_matrix();
		}

		for(u32 i = 0; i < skeleton.bone_count; i++)
			tmp_bone_ws[i] = skeleton.bone_inv_bind_matrices[i] * tmp_bone_ws[i];

		renderer_world_update_skin(r_obj.renderer_objectID, &tmp_bone_ws[0], skeleton.bone_count);
	}
}

void Editor::draw_ui()
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

	import_popup_id = ImGui::GetID("Import GLTF");

	menubar_draw();

	for(auto& widget : widgets)
		widget->draw();

	if(ImGui::BeginPopupModal("Import GLTF", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static std::string gltf_path = "";
		ImGui::InputText("Path", &gltf_path);
		if(ImGui::Button("Import"))
		{
			gltf_import_context ctx{.world = world};
			import_gltf(ctx, gltf_path);
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if(ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::End();
}

void Editor::menubar_draw()
{
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::Button("Run"))
		{
		}

		if(ImGui::BeginMenu("File"))
		{
			if(ImGui::BeginMenu("Import"))
			{
				if(ImGui::MenuItem("GLTF"))
				{
					ImGui::OpenPopup(import_popup_id);
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

			if(ImGui::MenuItem("Fly", "Y", tool == ViewportTool::Fly))
			{
				widget_viewport->set_tool(ViewportTool::Fly);			
			}	

			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Settings"))
		{
			if(ImGui::BeginMenu("Display"))
			{
				if(ImGui::BeginMenu("Present mode"))
				{
					auto* pmode_var = cvar_get("r_present_mode");
					
					if(ImGui::MenuItem("FIFO", nullptr, pmode_var->int_v == GPU_PRESENT_MODE_FIFO))
					{
						cvar_set(pmode_var, GPU_PRESENT_MODE_FIFO);
					}

					if(ImGui::MenuItem("FIFO_RELAXED", nullptr, pmode_var->int_v == GPU_PRESENT_MODE_FIFO_RELAXED))
					{
						cvar_set(pmode_var, GPU_PRESENT_MODE_FIFO_RELAXED);
					}

					if(ImGui::MenuItem("IMMEDIATE", nullptr, pmode_var->int_v == GPU_PRESENT_MODE_IMMEDIATE))
					{
						cvar_set(pmode_var, GPU_PRESENT_MODE_IMMEDIATE);
					}

					ImGui::EndMenu();
				}

				if(ImGui::BeginMenu("Window mode"))
				{
					auto* wmode_var = cvar_get("r_fullscreen");
					if(ImGui::MenuItem("Windowed", nullptr, wmode_var->int_v == 0))
					{
						cvar_set(wmode_var, 0);
					}

					if(ImGui::MenuItem("Borderless fullscreen", nullptr, wmode_var->int_v == 1)) 
					{
						cvar_set(wmode_var, 1);
					}

					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Debug"))
		{
			auto* vbdebug_cvar = cvar_get("r_visbuffer_debug");
			if(ImGui::MenuItem("Visbuffer", nullptr, vbdebug_cvar->int_v > 0))
			{
				cvar_set(vbdebug_cvar, vbdebug_cvar->int_v > 0 ? 0 : 1);
			}

			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("View"))
		{
			if(ImGui::MenuItem("Console", "~", console->is_open()))
			{
				console->set_open(!console->is_open());
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void Editor::create_rendertarget()
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

void Editor::update_main_camera()
{
	auto& camera_transform = world->entities.get<Transform>(world->main_camera);
	auto& camera = world->entities.get<camera_component>(world->main_camera);
	auto res = renderer_get_render_resolution();

	const float near = camera.near_plane;

	const float focal_length = 1.0f / std::tan(to_radians(camera.vertical_fov) / 2.0f);
	const float aspect_ratio = static_cast<float>(res.w) / static_cast<float>(res.h);
	const float x = focal_length / aspect_ratio;
	const float y = -focal_length;

	render_camera_data cam_data
	{
		.view = mat4::make_translation(-camera_transform.translation) * Quaternion::make_mat4(~camera_transform.rotation),
		.proj =
		{
			vec4{x,     0.0f,  0.0f,  0.0f},
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

void Editor::update_env()
{
	auto dlight = world->entities.get<directional_light_component>(world->env);
	renderer_update_environment
	({
		dlight.direction,
		dlight.color,
		dlight.intensity,
		1200.0f,
		world->r_envmap
	});
}

void Editor::try_patch_prefab(std::string_view name)
{
	if(name == "qmv_arena")
	{
		ecs::entity floor_ent;
		for(auto&& [entity, name] : world->entities.view<entity_name>().each())
		{
			if(name == "qmv_floor")
			{
				floor_ent = entity;
				break;
			}
		}

		auto floor_xform = world->entities.get<Transform>(floor_ent);

		physicsBodyDesc floor_desc
		{
			.initial_transform = floor_xform,
			.body_type = PHYSICS_BODY_STATIC
		};

		static physicsBoxHull floor_hull;
		floor_hull = physics_make_box_hull(vec3{25.0f, 0.05f, 25.0f});

		auto floor_rb = physics_create_body(floor_desc);
		auto floor_shape = physics_create_hull(floor_rb, {}, floor_hull.hull);
		world->entities.emplace<rigidbody_component>(floor_ent, floor_desc, floor_rb);
		world->entities.emplace<box_collider_component>(floor_ent, vec3{25.0f, 0.05f, 25.0f});

		auto test_entity = world->spawn("test_entity");
		add_entity_as_child(world->entities, world->root, test_entity);
		Transform te_xf{vec3{0.0f, 5.5f, 0.0f}, Quaternion{}, vec3{1.0f}};
		world->entities.emplace_or_replace<Transform>(test_entity, te_xf);

		ResourceID def_mat = resource_id_new(RESOURCE_TYPE_MATERIAL, 0);
		auto capsule = resource_manager_load_geometry("meshes/capsule");

		auto rd_object = renderer_world_insert_object
		({
			te_xf.as_matrix(),
			capsule,
			def_mat
		}, 3);
		world->entities.emplace<render_object_component>(test_entity, capsule, def_mat, rd_object);
		physicsBodyDesc te_rb_desc
		{
			.initial_transform = te_xf
		};

		auto caps_rb = physics_create_body(te_rb_desc);
		auto caps_shape = physics_create_capsule(caps_rb, {}, {.radius = 0.5f, .height = 1.0f});
		world->entities.emplace<rigidbody_component>(test_entity, te_rb_desc, caps_rb);
		world->entities.emplace<capsule_collider_component>(test_entity, physicsCapsule{.radius = 0.5f, .height = 1.0f});

		auto box = world->spawn("box");
		add_entity_as_child(world->entities, world->root, box);
		Transform bo_xf{vec3{0.0f, 1.5f, 0.0f}, Quaternion{}, vec3{1.0f}};
		world->entities.emplace_or_replace<Transform>(box, bo_xf);

		auto boxg = resource_manager_load_geometry("meshes/unit_cube");
		auto b_rd_object = renderer_world_insert_object
		({
			bo_xf.as_matrix(),
			boxg,
			def_mat
		}, 3);
		world->entities.emplace<render_object_component>(box, boxg, def_mat, b_rd_object);

		physicsBodyDesc box_rb_desc
		{
			.initial_transform = bo_xf
		};

		auto box_rb = physics_create_body(box_rb_desc);
		
		static physicsBoxHull box_hull;
		box_hull = physics_make_box_hull(vec3{0.5f, 0.5f, 0.5f});
		auto box_shape = physics_create_hull(box_rb, {}, box_hull.hull);
		world->entities.emplace<rigidbody_component>(box, box_rb_desc, box_rb);
		world->entities.emplace<box_collider_component>(box, vec3{0.5f, 0.5f, 0.5f});
	}
}

}

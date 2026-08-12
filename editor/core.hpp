#pragma once

#include <penumbra/gpu.hpp>
#include <penumbra/window.hpp>
#include <penumbra/ui.hpp>
#include <widgets/widget.hpp>
#include <memory>
#include <string>
#include <vector>

namespace penumbra
{

struct WorldState;
struct Viewport;
struct ConsoleWidget;

class Editor
{
public:
	Editor(window_t wnd, WorldState* ws, int argc, const char** argv);
	~Editor();

	void fixed_update(double dt);
	void variable_update(double dt);
	void draw_ui();
private:
	void create_rendertarget();
	void update_main_camera();
	void update_env();
	void menubar_draw();

	void try_patch_prefab(std::string_view name);

	window_t window;
	WorldState* world;
	std::vector<std::unique_ptr<Widget>> widgets;
	Viewport* widget_viewport;
	ConsoleWidget* console;

	uvec2 last_vp_size{800u, 600u};
	GPUTexture framebuffer_tex;
	GPUTextureDescriptor framebuffer;

	ImGuiID import_popup_id;
};

}

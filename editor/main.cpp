#include <tracy/Tracy.hpp>

import penumbra.core;
import penumbra.renderer;
import penumbra.ui;
import std;

using namespace penumbra;

int main(int argc, const char** argv)
{
	log_init();
	log::info("penumbra git-{}", config::git_hash);
	vfs_init();
	wm_init();

	{

	auto window = wm_create_window("penumbra_editor", {1280, 720});

	renderer_init(window);
	imgui_add_hook([&window](){ui::draw_device_overlay(window);});

	auto start = std::chrono::steady_clock::now();

	while(!window.requested_close())
	{
		ZoneScopedN("Main Loop");
		
		const auto end = std::chrono::steady_clock::now();
		const auto frame_time = end - start;
		start = end;
		
		renderer_next_frame();
		window.poll_events();
		renderer_process_frame(double(frame_time.count()) / 1e9);
		
		FrameMark;
	}

	renderer_shutdown();

	}
	
	wm_shutdown();
	vfs_shutdown();
}

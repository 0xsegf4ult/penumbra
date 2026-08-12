#include <penumbra/penumbra.hpp>

#include <core.hpp>
#include <world/state.hpp>

#include <tracy/Tracy.hpp>

#include <chrono>
#include <cmath>

using namespace penumbra;

static cvar_t fps_limit
{
	.name = "fps_max",
	.type = CVAR_TYPE_INT,
	.int_defv = 120,
	.int_v = 120
};

static cvar_t tickrate
{
	.name = "sv_tickrate",
	.type = CVAR_TYPE_INT,
	.int_defv = 60,
	.int_v = 60
};

int main(int argc, const char** argv)
{
	log_init();
	log::info("penumbra git-{}", config::git_hash);

	cvar_register(&fps_limit);
	cvar_register(&tickrate);

	vfs_init();
	wm_init();
	input_init();

	auto window = wm_create_window("penumbra_editor", {1280, 720});

	renderer_init(window);
	resource_manager_init();

	physics_create_world({});

	auto start = std::chrono::steady_clock::now();
	auto world_state = std::make_unique<WorldState>();
	auto editor = std::make_unique<Editor>(window, world_state.get(), argc, argv);

	std::chrono::steady_clock::duration accumulator{0};
	std::chrono::microseconds fixed_timestep{int(1.0 / double(tickrate.int_v) * 1e6)};

	while(!wm_requested_close())
	{
		ZoneScopedN("Main Loop");
		
		const auto end = std::chrono::steady_clock::now();
		const auto frame_time = end - start;
		start = end;
		accumulator += frame_time;

		renderer_next_frame();
		wm_poll_events();
		input_poll();

		while(accumulator >= fixed_timestep)
		{
			ZoneScopedN("Fixed Update");
			editor->fixed_update(double(fixed_timestep.count()) / 1e6);
			physics_world_simulate(double(fixed_timestep.count()) / 1e6, 4);
			accumulator -= fixed_timestep;
		}
		
		{
			ZoneScopedN("VRR Update");
			editor->variable_update(double(frame_time.count()) / 1e9);
		}
		renderer_process_frame(double(frame_time.count()) / 1e9);

		std::chrono::nanoseconds vrr_timestep{int(1.0 / double(fps_limit.int_v) * 1e9)};
		auto ft = std::chrono::steady_clock::now() - start;
		auto sleep_time = vrr_timestep - ft;
		if(vrr_timestep > ft)
		{
			ZoneScopedN("sleep");
			static double estimate = 5e-3;
			static double mean = 5e-3;
			static double m2 = 0;
			static int64_t count = 1;
			double seconds = double(sleep_time.count()) / 1e9;
			while(seconds > estimate)
			{
				auto slp_start = std::chrono::steady_clock::now();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				auto slp_end = std::chrono::steady_clock::now();

				double observed = (slp_end - slp_start).count() / 1e9;
				seconds -= observed;

				++count;
				double delta = observed - mean;
				mean += delta / count;
				m2 += delta * (observed - mean);
				double stddev = std::sqrt(m2 / (count - 1));
				estimate = mean + stddev;
			}

			auto spin_start = std::chrono::steady_clock::now();
			auto spinNS = int64_t(seconds * 1e9);
			auto delay = std::chrono::nanoseconds(spinNS);
			while(std::chrono::steady_clock::now() - spin_start < delay) {}
		}

		FrameMark;
	}

	gpu_wait_idle();
	editor.reset();

	physics_destroy_world();
	resource_manager_shutdown();
	renderer_shutdown();
	
	wm_destroy_window(window);

	input_shutdown();
	wm_shutdown();
	vfs_shutdown();
}

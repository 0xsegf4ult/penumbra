#include <penumbra/penumbra.hpp>
#include <chrono>
#include <tracy/Tracy.hpp>

namespace penumbra
{

static u32 subsystems;
static window_t main_window;

static cvar_t fps_limit
{
	.name = "fps_max",
	.type = CVAR_TYPE_INT,
	.int_defv = 120
};

static cvar_t tickrate
{
	.name = "sv_tickrate",
	.type = CVAR_TYPE_INT,
	.int_defv = 60
};

bool penumbra_init(const penumbra_init_config& cfg)
{
	subsystems = cfg.subsystems;
	if(subsystems & PENUMBRA_SUBSYSTEM_INPUT)
		subsystems |= PENUMBRA_SUBSYSTEM_WINDOW;
	if(subsystems & PENUMBRA_SUBSYSTEM_UI)
		subsystems |= PENUMBRA_SUBSYSTEM_WINDOW | PENUMBRA_SUBSYSTEM_INPUT | PENUMBRA_SUBSYSTEM_GPU;
	if(subsystems & PENUMBRA_SUBSYSTEM_RENDERER)
		subsystems |= PENUMBRA_SUBSYSTEM_WINDOW | PENUMBRA_SUBSYSTEM_INPUT | PENUMBRA_SUBSYSTEM_GPU | PENUMBRA_SUBSYSTEM_RESOURCE | PENUMBRA_SUBSYSTEM_UI;

	log_init();
	log::info("penumbra git-{}", config::git_hash);

	cvar_register(&fps_limit);
	cvar_register(&tickrate);

	vfs_init();

	if(subsystems & PENUMBRA_SUBSYSTEM_WINDOW)
		wm_init();
	if(subsystems & PENUMBRA_SUBSYSTEM_INPUT)
		input_init();
	if(subsystems & PENUMBRA_SUBSYSTEM_AUDIO)
		audio_init();

	if(subsystems & PENUMBRA_SUBSYSTEM_WINDOW)
		main_window = wm_create_window(cfg.window_title, {1280, 720});

	if(subsystems & PENUMBRA_SUBSYSTEM_RENDERER)
		renderer_init(main_window);
	
	if(subsystems & PENUMBRA_SUBSYSTEM_UI)
		ui::device_overlay_init();
	
	if(subsystems & PENUMBRA_SUBSYSTEM_RESOURCE)
		resource_manager_init();

	if(subsystems & PENUMBRA_SUBSYSTEM_PHYSICS)
		physics_init();

	return true;
}

void penumbra_shutdown()
{
	if(subsystems & PENUMBRA_SUBSYSTEM_PHYSICS)
		physics_shutdown();

	if(subsystems & PENUMBRA_SUBSYSTEM_RESOURCE)
		resource_manager_shutdown();

	if(subsystems & PENUMBRA_SUBSYSTEM_RENDERER)
		renderer_shutdown();

	if(subsystems & PENUMBRA_SUBSYSTEM_WINDOW)
		wm_destroy_window(main_window);

	if(subsystems & PENUMBRA_SUBSYSTEM_AUDIO)
		audio_shutdown();

	if(subsystems & PENUMBRA_SUBSYSTEM_INPUT)
		input_shutdown();

	if(subsystems & PENUMBRA_SUBSYSTEM_WINDOW)
		wm_shutdown();

	vfs_shutdown();
}

static bool should_run()
{
	if(subsystems & PENUMBRA_SUBSYSTEM_WINDOW)
		return !wm_requested_close();

	return true;
}

void penumbra_run(void* state, prepare_update_t pupd, fixed_update_t fupd, variable_update_t vupd)
{
	auto start = std::chrono::steady_clock::now();
	std::chrono::steady_clock::duration accumulator{0};
	std::chrono::microseconds fixed_timestep{int(1.0 / double(tickrate.int_v) * 1e6)};

	while(should_run())
	{
		ZoneScopedN("Main Loop");

		const auto end = std::chrono::steady_clock::now();
		const auto frame_time = end - start;
		start = end;
		accumulator += frame_time;

		if(subsystems & PENUMBRA_SUBSYSTEM_RENDERER)
			renderer_next_frame();
		if(subsystems & PENUMBRA_SUBSYSTEM_WINDOW)
			wm_poll_events();
		if(subsystems & PENUMBRA_SUBSYSTEM_INPUT)
			input_poll();

		pupd(state);

		while(accumulator >= fixed_timestep)
		{
			ZoneScopedN("Fixed Update");
			fupd(state, fixed_timestep.count() / 1e6f);
			if(subsystems & PENUMBRA_SUBSYSTEM_PHYSICS)
				physics_world_simulate(fixed_timestep.count() / 1e6f, 4);

			accumulator -= fixed_timestep;
		}

		{
			ZoneScopedN("VRR Update");
			vupd(state, frame_time.count() / 1e9);
		}

		if(subsystems & PENUMBRA_SUBSYSTEM_RENDERER)
			renderer_process_frame(frame_time.count() / 1e9);

		if(fps_limit.int_v > 0)
		{
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
		}

		FrameMark;
	}

	if(subsystems & PENUMBRA_SUBSYSTEM_GPU)
		gpu_wait_idle();
}

window_t penumbra_get_window()
{
	return main_window;
}

}

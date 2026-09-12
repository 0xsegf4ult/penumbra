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

static void prepare_update(void* state)
{
}

static void call_fupd(void* state, float dt)
{
	((Editor*)state)->fixed_update(dt);
}

static void call_vupd(void* state, float dt)
{
	((Editor*)state)->variable_update(dt);
}

int main(int argc, const char** argv)
{
	penumbra_init({.window_title = "penumbra_editor"});
	auto world_state = std::make_unique<WorldState>();
	auto editor = std::make_unique<Editor>(penumbra_get_window(), world_state.get(), argc, argv);

	penumbra_run(editor.get(), prepare_update, call_fupd, call_vupd);

	editor.reset();

	penumbra_shutdown();
}

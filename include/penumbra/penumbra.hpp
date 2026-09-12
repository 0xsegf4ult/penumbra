#pragma once

#include <penumbra/array_proxy.hpp>
#include <penumbra/audio.hpp>
#include <penumbra/config.hpp>
#include <penumbra/cmd.hpp>
#include <penumbra/cvar.hpp>
#include <penumbra/ecs.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/hash.hpp>
#include <penumbra/input.hpp>
#include <penumbra/input_keys.hpp>
#include <penumbra/log.hpp>
#include <penumbra/math.hpp>
#include <penumbra/panic.hpp>
#include <penumbra/physics.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/resource.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/types.hpp>
#include <penumbra/ui.hpp>
#include <penumbra/vfs.hpp>
#include <penumbra/window.hpp>

namespace penumbra
{

enum penumbra_subsystem : u32
{
	PENUMBRA_SUBSYSTEM_AUDIO 	= 0x1,
	PENUMBRA_SUBSYSTEM_GPU		= 0x2,
	PENUMBRA_SUBSYSTEM_INPUT	= 0x4,
	PENUMBRA_SUBSYSTEM_PHYSICS 	= 0x8,
	PENUMBRA_SUBSYSTEM_RENDERER  	= 0x10,
	PENUMBRA_SUBSYSTEM_RESOURCE	= 0x20,
	PENUMBRA_SUBSYSTEM_UI		= 0x40,
	PENUMBRA_SUBSYSTEM_WINDOW	= 0x80,
	PENUMBRA_SUBSYSTEM_ALL 		= 0xFFFFFFFF
};

typedef void (*prepare_update_t)(void* state);
typedef void (*fixed_update_t)(void* state, float dt);
typedef void (*variable_update_t)(void* state, float dt);

struct penumbra_init_config
{
	u32 subsystems{PENUMBRA_SUBSYSTEM_ALL};
	const char* window_title = "penumbra";
};

bool penumbra_init(const penumbra_init_config& cfg);
void penumbra_shutdown();
void penumbra_run(void* state, prepare_update_t pupd, fixed_update_t fupd, variable_update_t vupd);
window_t penumbra_get_window();

}

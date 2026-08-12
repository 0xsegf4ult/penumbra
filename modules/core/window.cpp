#include <penumbra/window.hpp>
#include <penumbra/input.hpp>
#include <penumbra/panic.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/types.hpp>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_messagebox.h>

namespace penumbra
{

static bool wants_quit = false;

void wm_init()
{
	SDL_SetMainReady();
	SDL_Init(SDL_INIT_VIDEO);
}

void wm_shutdown()
{
	SDL_Quit();
}

void wm_message_box(const char* title, const char* message, WM_MSGBOX_TYPE type)
{
	SDL_MessageBoxFlags flags{0};
	switch(type)
	{
	case WM_MESSAGE_BOX_INFO:
		flags = SDL_MESSAGEBOX_INFORMATION;
		break;
	case WM_MESSAGE_BOX_WARNING:
		flags = SDL_MESSAGEBOX_WARNING;
		break;
	case WM_MESSAGE_BOX_ERROR:
		flags = SDL_MESSAGEBOX_ERROR;
		break;
	}

	SDL_ShowSimpleMessageBox(flags, title, message, nullptr);
}

window_t wm_create_window(const char* title, uvec2 wnd_size)
{
	SDL_Window* wnd = SDL_CreateWindow(title, wnd_size.w, wnd_size.h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
	if(!wnd)
		panic("Failed to create window");

	return reinterpret_cast<window_t>(wnd);
}

void wm_destroy_window(window_t window)
{
	auto* wnd = reinterpret_cast<SDL_Window*>(window);
	SDL_DestroyWindow(wnd);
}

void wm_poll_events()
{
	SDL_Event event;

	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_EVENT_QUIT:
		{
			wants_quit = true;
			break;
		}
		default:
			input_dispatch_event(event);
			break;
		}
	}
}

bool wm_requested_close()
{
	return wants_quit;
}

uvec2 wm_get_size(window_t window)
{
	int w, h;
	SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(window), &w, &h);
	return {static_cast<u32>(w), static_cast<u32>(h)};
}

bool wm_is_fullscreen(window_t window)
{
	return SDL_GetWindowFlags(reinterpret_cast<SDL_Window*>(window)) & SDL_WINDOW_FULLSCREEN;
}

void wm_set_fullscreen(window_t window, bool state)
{
	SDL_SetWindowFullscreen(reinterpret_cast<SDL_Window*>(window), state);
}

}

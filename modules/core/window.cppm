module;

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_messagebox.h>
#include <tracy/Tracy.hpp>

export module penumbra.core:window;
import penumbra.math;
import :log;

import std;

export namespace penumbra
{

void wm_init()
{
	SDL_SetMainReady();
	SDL_Init(SDL_INIT_VIDEO);
}

void wm_shutdown()
{
	SDL_Quit();
}

enum MessageBoxType
{
	WM_MESSAGE_BOX_INFO,
	WM_MESSAGE_BOX_WARNING,
	WM_MESSAGE_BOX_ERROR
};

void wm_message_box(const char* title, const char* message, MessageBoxType type = WM_MESSAGE_BOX_INFO)
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

enum class input_mouse_button
{
	left = 1,
	middle = 2,
	right = 3,
	side1 = 4,
	side2 = 5
};

using text_event_callback = std::function<void(const char*)>;
using mouse_button_event_callback = std::function<void(input_mouse_button, bool)>;
using mouse_move_event_callback = std::function<void(float x, float y, float dx, float dy)>;
using mouse_wheel_event_callback = std::function<void(float x, float y)>;

class Window
{
public:
	Window(SDL_Window* wnd, uvec2 wnd_size) : handle{wnd}, size{wnd_size} {}
	~Window()
	{
		if(handle)
			SDL_DestroyWindow(handle);
	}

	bool requested_close() const
	{
		return req_close;
	}

	void poll_events() noexcept
	{
		ZoneScoped;

		SDL_Event event;

		float mx, my;
		SDL_GetGlobalMouseState(&mx, &my);
		vec2 mpos{mx, my};

		mouse_delta = mpos - mouse_pos;
		mouse_pos = mpos;

		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
			case SDL_EVENT_QUIT:
			{
				req_close = true;
				break;
			}
			case SDL_EVENT_WINDOW_RESIZED:
			{
				size.w = static_cast<uint32_t>(event.window.data1);
				size.h = static_cast<uint32_t>(event.window.data2);
				break;
			}
			case SDL_EVENT_TEXT_INPUT:
			{
				for(auto& callback: text_event_listeners)
					callback(event.text.text);

				break;
			}
			case SDL_EVENT_MOUSE_MOTION:
			{
				for(auto& callback: mouse_move_event_listeners)
					callback(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				for(auto& callback : mouse_button_event_listeners)
					callback(input_mouse_button{event.button.button}, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
				break;
			}
			case SDL_EVENT_MOUSE_WHEEL:
			{
				for(auto& callback : mouse_wheel_event_listeners)
					callback(event.wheel.x, event.wheel.y);

				break;
			}
			default:
				break;
			}
		}
	}

	vec2 get_mouse_pos() const
	{
		return mouse_pos;
	}

	vec2 get_mouse_delta() const
	{
		return mouse_delta;
	}

	SDL_Window* native_handle() const
	{
		return handle;
	}

	uvec2 get_size() const
	{
		return size;
	}

	bool is_fullscreen() const
	{
		return SDL_GetWindowFlags(handle) & SDL_WINDOW_FULLSCREEN;
	}

	void set_fullscreen(bool state)
	{
		SDL_SetWindowFullscreen(handle, state);
	}

        void register_text_event_listener(text_event_callback callback)
        {
                text_event_listeners.push_back(callback);
        }

        void register_mouse_button_event_listener(mouse_button_event_callback callback)
        {
                mouse_button_event_listeners.push_back(callback);
        }

        void register_mouse_move_event_listener(mouse_move_event_callback callback)
        {
                mouse_move_event_listeners.push_back(callback);
        }

        void register_mouse_wheel_event_listener(mouse_wheel_event_callback callback)
        {
                mouse_wheel_event_listeners.push_back(callback);
        }

	bool text_input_active() const
	{
		return SDL_TextInputActive(handle);
	}

	void start_text_input()
	{
		SDL_StartTextInput(handle);
	}

	void stop_text_input()
	{
		SDL_StopTextInput(handle);
	}
private:
	SDL_Window* handle{nullptr};
	uvec2 size;

	bool req_close{false};
	
	vec2 mouse_pos{0.0f};
	vec2 mouse_delta{0.0f};

	std::vector<text_event_callback> text_event_listeners;
        std::vector<mouse_button_event_callback> mouse_button_event_listeners;
        std::vector<mouse_move_event_callback> mouse_move_event_listeners;
        std::vector<mouse_wheel_event_callback> mouse_wheel_event_listeners;
};

Window wm_create_window(const char* title, uvec2 wnd_size)
{
	SDL_Window* wnd = SDL_CreateWindow(title, wnd_size.w, wnd_size.h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
	if(!wnd)
	{
		log::critical("window_manager: failed to create window");
		wm_message_box("Fatal error", "Failed to create window", WM_MESSAGE_BOX_ERROR);
		std::terminate();		
	}

	return {wnd, wnd_size};
}

}

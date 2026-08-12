#pragma once

#include <penumbra/input_keys.hpp>
#include <penumbra/types.hpp>
#include <penumbra/window.hpp>

#include <functional>

union SDL_Event;

namespace penumbra
{

void input_init();
void input_shutdown();
void input_poll();

void input_dispatch_event(const SDL_Event& event);

enum input_event_type
{
	INPUT_EVENT_KEY_DOWN,
	INPUT_EVENT_KEY_UP,
	INPUT_EVENT_TEXT_INPUT,
	INPUT_EVENT_MOUSE_MOTION,
	INPUT_EVENT_MOUSE_BUTTON_DOWN,
	INPUT_EVENT_MOUSE_BUTTON_UP,
	INPUT_EVENT_MOUSE_WHEEL
};

struct input_event_t
{
	input_event_type type;

	union
	{
		struct
		{
			kbd_scancode scancode;
		} key;

		struct
		{
			const char* data;
		} text;

		struct
		{
			vec2 pos;
			vec2 delta;
		} mouse_motion;

		struct
		{
			u8 button;
		} mouse_button;

		struct
		{
			vec2 delta;
		} mouse_wheel;
	};
};

using input_listener_t = std::function<void(const input_event_t&)>;
void input_register_listener(const input_listener_t& listener);

bool input_is_key_down(kbd_scancode key);
bool input_text_input_active();
bool input_start_text_input();
bool input_stop_text_input();
void input_set_mouse_capture(bool state);
vec2 input_get_mouse_pos();
vec2 input_get_mouse_delta();

}

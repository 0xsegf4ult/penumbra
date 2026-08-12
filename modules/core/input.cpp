#include <penumbra/input.hpp>
#include <penumbra/window.hpp>
#include <penumbra/types.hpp>
#include <penumbra/math/vector.hpp>

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <cassert>
#include <vector>

namespace penumbra
{

struct input_state_t
{
	SDL_Window* window;

	vec2 mouse_pos{0.0f};
	vec2 mouse_delta{0.0f};
	const bool* key_states{nullptr};

	std::vector<input_listener_t> listeners;

	bool capture_mouse{false};
};

static input_state_t* input_state = nullptr;

void input_init()
{
	input_state = new input_state_t();
	input_state->key_states = SDL_GetKeyboardState(nullptr);
}

void input_shutdown()
{
	assert(input_state);

	delete input_state;
	input_state = nullptr;
}

void input_poll()
{
	assert(input_state);

	if(input_state->capture_mouse)
	{
		float dx, dy;
		SDL_GetRelativeMouseState(&dx, &dy);
		vec2 delta{dx, dy};
		input_state->mouse_delta = delta;
		input_state->mouse_pos += delta;
	}
	else
	{
		float mx, my;
		SDL_GetGlobalMouseState(&mx, &my);

		vec2 mpos{mx, my};
		input_state->mouse_delta = mpos - input_state->mouse_pos;
		input_state->mouse_pos = mpos;
	}
}

static void listener_dispatch(const input_event_t& event)
{
	assert(input_state);
	for(auto& callback : input_state->listeners)
		callback(event);
}

void input_dispatch_event(const SDL_Event& event)
{
	assert(input_state);

	switch(event.type)
	{
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		input_state->window = SDL_GetWindowFromID(event.window.windowID);
		break;
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		listener_dispatch
		({
			.type = (event.type == SDL_EVENT_KEY_DOWN) ? INPUT_EVENT_KEY_DOWN : INPUT_EVENT_KEY_UP,
			.key =
			{
				.scancode = sdl_scancode_parse(event.key.scancode)
			}
		});
		break;
	case SDL_EVENT_TEXT_INPUT:
		listener_dispatch
		({
			.type = INPUT_EVENT_TEXT_INPUT,
			.text =
			{
				.data = event.text.text
			}
		});
		break;
	case SDL_EVENT_MOUSE_MOTION:
		listener_dispatch
		({
			.type = INPUT_EVENT_MOUSE_MOTION,
			.mouse_motion =
			{
				.pos = vec2{event.motion.x, event.motion.y},
				.delta = vec2{event.motion.xrel, event.motion.yrel}
			}
		});
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		listener_dispatch
		({
			.type = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP,
			.mouse_button =
			{
				.button = sdl_mouse_parse(event.button.button)
			}
		});
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		listener_dispatch
		({
			.type = INPUT_EVENT_MOUSE_WHEEL,
			.mouse_wheel =
			{
				.delta = vec2{event.wheel.x, event.wheel.y}
			}
		});
		break;
	default:
		break;
	}
}

void input_register_listener(const input_listener_t& listener)
{
	assert(input_state);
	input_state->listeners.push_back(listener);
}

bool input_is_key_down(kbd_scancode key)
{
	assert(input_state);

	if(key >= SCANCODE_COUNT)
		return false;

	return input_state->key_states[scancode_to_sdl(key)];
}

bool input_text_input_active()
{
	assert(input_state);
	return SDL_TextInputActive(input_state->window);
}

bool input_start_text_input()
{
	assert(input_state);
	return SDL_StartTextInput(input_state->window);
}

bool input_stop_text_input()
{
	assert(input_state);
	return SDL_StopTextInput(input_state->window);
}

void input_set_mouse_capture(bool state)
{
	assert(input_state);
	input_state->capture_mouse = state;
	SDL_SetWindowRelativeMouseMode(input_state->window, state);
	if(state)
		SDL_GetRelativeMouseState(nullptr, nullptr);
}

vec2 input_get_mouse_pos()
{
	assert(input_state);
	return input_state->mouse_pos;
}

vec2 input_get_mouse_delta()
{
	assert(input_state);
	return input_state->mouse_delta;
}

}

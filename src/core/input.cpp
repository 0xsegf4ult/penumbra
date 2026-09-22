#include <penumbra/input.hpp>
#include <penumbra/window.hpp>
#include <penumbra/cmd.hpp>
#include <penumbra/types.hpp>
#include <penumbra/math/vector.hpp>

#include <core/input_translate.hpp>

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <cassert>
#include <format>
#include <string_view>
#include <vector>

#include <tracy/Tracy.hpp>

namespace penumbra
{

struct key_binding_t
{
	cmd_t* event_down{nullptr};
	cmd_t* event_up{nullptr};
};

struct input_state_t
{
	SDL_Window* window;

	vec2 mouse_pos{0.0f};
	vec2 mouse_delta{0.0f};
	const bool* key_states{nullptr};

	std::vector<input_listener_t> listeners;
	bool mouse_buttons[5];

	bool capture_mouse{false};

	key_binding_t bindings[NUM_INPUT_KEYS];
};

static input_state_t* input_state = nullptr;

static void bind_cmd_cb(cmd_args_t args)
{
	if(args.size() < 2)
		return;

	keycode_t key = string_to_keycode(args[0]);
	if(key == KEY_NONE)
		return;

	cmd_t* down_cmd = cmd_get(args[1]);
	if(!down_cmd)
		return;

	cmd_t* up_cmd = nullptr;
	if(args[1][0] == '+')
	{
		if(args[1].length() < 2)
			return;

		auto paired = std::format("-{}", args[1].substr(1));
		up_cmd = cmd_get(paired);
		if(!up_cmd)
			return;
	}

	auto& binding = input_state->bindings[key];
	binding.event_down = down_cmd;
	binding.event_up = up_cmd;
}

static void unbind_cmd_cb(cmd_args_t args)
{
	if(args.size() < 1)
		return;

	keycode_t key = string_to_keycode(args[0]);
	if(key == KEY_NONE)
		return;

	auto& binding = input_state->bindings[key];
	binding.event_down = nullptr;
	binding.event_up = nullptr;
}

static cmd_t bind_cmd
{
	.name = "bind",
	.callback = bind_cmd_cb
};

static cmd_t unbind_cmd
{
	.name = "unbind",
	.callback = unbind_cmd_cb
};

void input_init()
{
	input_state = new input_state_t();
	input_state->key_states = SDL_GetKeyboardState(nullptr);

	cmd_register(&bind_cmd);
	cmd_register(&unbind_cmd);
}

void input_shutdown()
{
	assert(input_state);

	delete input_state;
	input_state = nullptr;
}

void input_poll()
{
	ZoneScoped;
	assert(input_state);

	SDL_MouseButtonFlags buttons;

	if(input_state->capture_mouse)
	{
		float dx, dy;
		buttons = SDL_GetRelativeMouseState(&dx, &dy);
		vec2 delta{dx, dy};
		input_state->mouse_delta = delta;
		input_state->mouse_pos += delta;
	}
	else
	{
		float mx, my;
		buttons = SDL_GetGlobalMouseState(&mx, &my);

		vec2 mpos{mx, my};
		input_state->mouse_delta = mpos - input_state->mouse_pos;
		input_state->mouse_pos = mpos;
	}

	input_state->mouse_buttons[0] = buttons & SDL_BUTTON_LMASK;
	input_state->mouse_buttons[1] = buttons & SDL_BUTTON_RMASK;
	input_state->mouse_buttons[2] = buttons & SDL_BUTTON_MMASK;
	input_state->mouse_buttons[3] = buttons & SDL_BUTTON_X1MASK;
	input_state->mouse_buttons[4] = buttons & SDL_BUTTON_X2MASK;
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
	{
		bool down = (event.type == SDL_EVENT_KEY_DOWN);
		keycode_t key = sdl_scancode_parse(event.key.scancode);
		
		auto& binding = input_state->bindings[key];

		if(down && binding.event_down)
			binding.event_down->callback({});
		else if(binding.event_up)
			binding.event_up->callback({});

		listener_dispatch
		({
			.type = down ? INPUT_EVENT_KEY_DOWN : INPUT_EVENT_KEY_UP,
			.key =
			{
				.scancode = key 
			}
		});
		break;
	}
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
	{
		bool down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
		keycode_t key = sdl_mouse_parse(event.button.button);

		auto& binding = input_state->bindings[key];
		if(down && binding.event_down)
			binding.event_down->callback({});
		else if(binding.event_up)
			binding.event_up->callback({});

		listener_dispatch
		({
			.type = down ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP,
			.mouse_button =
			{
				.button = key
			}
		});
		break;
	}
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

bool input_is_key_down(keycode_t key)
{
	assert(input_state);

	if(key >= MOUSE_LEFT && key <= MOUSE_5)
		return input_state->mouse_buttons[key - MOUSE_LEFT];
	else if(key >= NUM_INPUT_KEYS)
		return false;

	return input_state->key_states[keycode_to_sdl_key(key)];
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

bool input_is_mouse_down(u8 button)
{
	assert(input_state);
	return input_state->mouse_buttons[button - 1];
}

}

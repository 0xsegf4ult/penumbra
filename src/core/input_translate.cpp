#include <penumbra/input_keys.hpp>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_mouse.h>
#include <array>
#include <cctype>
#include <string_view>

namespace penumbra
{

constexpr auto sdl_to_key_lookup = []()
{
	std::array<keycode_t, SDL_SCANCODE_COUNT> data{};

	data[SDL_SCANCODE_0] = KEY_0;
	data[SDL_SCANCODE_1] = KEY_1;
	data[SDL_SCANCODE_2] = KEY_2;
	data[SDL_SCANCODE_3] = KEY_3;
	data[SDL_SCANCODE_4] = KEY_4;
	data[SDL_SCANCODE_5] = KEY_5;
	data[SDL_SCANCODE_6] = KEY_6;
	data[SDL_SCANCODE_7] = KEY_7;
	data[SDL_SCANCODE_8] = KEY_8;
	data[SDL_SCANCODE_9] = KEY_9;
	data[SDL_SCANCODE_A] = KEY_A;
	data[SDL_SCANCODE_B] = KEY_B;
	data[SDL_SCANCODE_C] = KEY_C;
	data[SDL_SCANCODE_D] = KEY_D;
	data[SDL_SCANCODE_E] = KEY_E;
	data[SDL_SCANCODE_F] = KEY_F;
	data[SDL_SCANCODE_G] = KEY_G;
	data[SDL_SCANCODE_H] = KEY_H;
	data[SDL_SCANCODE_I] = KEY_I;
	data[SDL_SCANCODE_J] = KEY_J;
	data[SDL_SCANCODE_K] = KEY_K;
	data[SDL_SCANCODE_L] = KEY_L;
	data[SDL_SCANCODE_M] = KEY_M;
	data[SDL_SCANCODE_N] = KEY_N;
	data[SDL_SCANCODE_O] = KEY_O;
	data[SDL_SCANCODE_P] = KEY_P;
	data[SDL_SCANCODE_Q] = KEY_Q;
	data[SDL_SCANCODE_R] = KEY_R;
	data[SDL_SCANCODE_S] = KEY_S;
	data[SDL_SCANCODE_T] = KEY_T;
	data[SDL_SCANCODE_U] = KEY_U;
	data[SDL_SCANCODE_V] = KEY_V;
	data[SDL_SCANCODE_W] = KEY_W;
	data[SDL_SCANCODE_X] = KEY_X;
	data[SDL_SCANCODE_Y] = KEY_Y;
	data[SDL_SCANCODE_Z] = KEY_Z;
	data[SDL_SCANCODE_RETURN] = KEY_ENTER;
	data[SDL_SCANCODE_ESCAPE] = KEY_ESCAPE;
	data[SDL_SCANCODE_BACKSPACE] = KEY_BACKSPACE;
	data[SDL_SCANCODE_TAB] = KEY_TAB;
	data[SDL_SCANCODE_SPACE] = KEY_SPACE;
	data[SDL_SCANCODE_MINUS] = KEY_MINUS;
	data[SDL_SCANCODE_EQUALS] = KEY_EQUAL;
	data[SDL_SCANCODE_LEFTBRACKET] = KEY_LBRACKET;
	data[SDL_SCANCODE_RIGHTBRACKET] = KEY_RBRACKET;
	data[SDL_SCANCODE_BACKSLASH] = KEY_BACKSLASH;
	data[SDL_SCANCODE_SEMICOLON] = KEY_SEMICOLON;
	data[SDL_SCANCODE_APOSTROPHE] = KEY_APOSTROPHE;
	data[SDL_SCANCODE_GRAVE] = KEY_TILDE;
	data[SDL_SCANCODE_COMMA] = KEY_COMMA;
	data[SDL_SCANCODE_PERIOD] = KEY_PERIOD;
	data[SDL_SCANCODE_SLASH] = KEY_SLASH;
	data[SDL_SCANCODE_CAPSLOCK] = KEY_CAPSLOCK;
	data[SDL_SCANCODE_F1] = KEY_F1;
	data[SDL_SCANCODE_F2] = KEY_F2;
	data[SDL_SCANCODE_F3] = KEY_F3;
	data[SDL_SCANCODE_F4] = KEY_F4;
	data[SDL_SCANCODE_F5] = KEY_F5;
	data[SDL_SCANCODE_F6] = KEY_F6;
	data[SDL_SCANCODE_F7] = KEY_F7;
	data[SDL_SCANCODE_F8] = KEY_F8;
	data[SDL_SCANCODE_F9] = KEY_F9;
	data[SDL_SCANCODE_F10] = KEY_F10;
	data[SDL_SCANCODE_F11] = KEY_F11;
	data[SDL_SCANCODE_F12] = KEY_F12;
	data[SDL_SCANCODE_SCROLLLOCK] = KEY_SCROLLLOCK;
	data[SDL_SCANCODE_PAUSE] = KEY_BREAK;
	data[SDL_SCANCODE_INSERT] = KEY_INSERT;
	data[SDL_SCANCODE_HOME] = KEY_HOME;
	data[SDL_SCANCODE_PAGEUP] = KEY_PAGEUP;
	data[SDL_SCANCODE_DELETE] = KEY_DELETE;
	data[SDL_SCANCODE_END] = KEY_END;
	data[SDL_SCANCODE_PAGEDOWN] = KEY_PAGEDOWN;
	data[SDL_SCANCODE_RIGHT] = KEY_RIGHT;
	data[SDL_SCANCODE_LEFT] = KEY_LEFT;
	data[SDL_SCANCODE_DOWN] = KEY_DOWN;
	data[SDL_SCANCODE_UP] = KEY_UP;
	data[SDL_SCANCODE_NUMLOCKCLEAR] = KEY_NUMLOCK;
	data[SDL_SCANCODE_KP_DIVIDE] = KEY_KP_DIVIDE;
	data[SDL_SCANCODE_KP_MULTIPLY] = KEY_KP_MULTIPLY;
	data[SDL_SCANCODE_KP_MINUS] = KEY_KP_MINUS;
	data[SDL_SCANCODE_KP_PLUS] = KEY_KP_PLUS;
	data[SDL_SCANCODE_KP_ENTER] = KEY_KP_ENTER;
	data[SDL_SCANCODE_KP_PERIOD] = KEY_KP_PERIOD;
	data[SDL_SCANCODE_KP_0] = KEY_KP0;
	data[SDL_SCANCODE_KP_1] = KEY_KP1;
	data[SDL_SCANCODE_KP_2] = KEY_KP2;
	data[SDL_SCANCODE_KP_3] = KEY_KP3;
	data[SDL_SCANCODE_KP_4] = KEY_KP4;
	data[SDL_SCANCODE_KP_5] = KEY_KP5;
	data[SDL_SCANCODE_KP_6] = KEY_KP6;
	data[SDL_SCANCODE_KP_7] = KEY_KP7;
	data[SDL_SCANCODE_KP_8] = KEY_KP8;
	data[SDL_SCANCODE_KP_9] = KEY_KP9;
	data[SDL_SCANCODE_LCTRL] = KEY_LCONTROL;
	data[SDL_SCANCODE_LSHIFT] = KEY_LSHIFT;
	data[SDL_SCANCODE_LALT] = KEY_LALT;
	data[SDL_SCANCODE_LGUI] = KEY_LGUI;
	data[SDL_SCANCODE_RCTRL] = KEY_RCONTROL;
	data[SDL_SCANCODE_RSHIFT] = KEY_RSHIFT;
	data[SDL_SCANCODE_RALT] = KEY_RALT;
	data[SDL_SCANCODE_RGUI] = KEY_RGUI;
	data[SDL_SCANCODE_APPLICATION] = KEY_APP;

	return data;
}();

constexpr auto key_to_sdl_lookup = []()
{
	std::array<SDL_Scancode, SDL_SCANCODE_COUNT> data{};

	for(int i = 0; i < SDL_SCANCODE_COUNT; i++)
		data[sdl_to_key_lookup[i]] = SDL_Scancode(i);

	return data;
}();	

keycode_t sdl_scancode_parse(SDL_Scancode scancode)
{
	return sdl_to_key_lookup[scancode]; 
}

SDL_Scancode keycode_to_sdl_key(keycode_t key)
{
	return key_to_sdl_lookup[key];
}

keycode_t sdl_mouse_parse(u8 button)
{
	switch(button)
	{
	case SDL_BUTTON_LEFT:
		return MOUSE_LEFT;
	case SDL_BUTTON_MIDDLE:
		return MOUSE_MIDDLE;
	case SDL_BUTTON_RIGHT: 
		return MOUSE_RIGHT;
	case SDL_BUTTON_X1:
		return MOUSE_4;
	case SDL_BUTTON_X2:
		return MOUSE_5;
	default:
		return KEY_NONE;
	}
}

struct keyname_t
{
	std::string_view name;
	keycode_t key;
};

constexpr keyname_t key_names[] =
{
	{"enter", KEY_ENTER},
	{"escape", KEY_ESCAPE},
	{"backspace", KEY_BACKSPACE},
	{"tab", KEY_TAB},
	{"space", KEY_SPACE},
	{"-", KEY_MINUS},
	{"=", KEY_EQUAL},
	{"[", KEY_LBRACKET},
	{"]", KEY_RBRACKET},
	{"\\", KEY_BACKSLASH},
	{"semicolon", KEY_SEMICOLON},
	{"'", KEY_APOSTROPHE},
	{"`", KEY_TILDE},
	{",", KEY_COMMA},
	{".", KEY_PERIOD},
	{"/", KEY_SLASH},
	{"capslock", KEY_CAPSLOCK},
	{"f1", KEY_F1},
	{"f2", KEY_F2},
	{"f3", KEY_F3},
	{"f4", KEY_F4},
	{"f5", KEY_F5},
	{"f6", KEY_F6},
	{"f7", KEY_F7},
	{"f8", KEY_F8},
	{"f9", KEY_F9},
	{"f10", KEY_F10},
	{"f11", KEY_F11},
	{"f12", KEY_F12},
	{"scrolllock", KEY_SCROLLLOCK},
	{"pause", KEY_BREAK},
	{"ins", KEY_INSERT},
	{"home", KEY_HOME},
	{"pgup", KEY_PAGEUP},
	{"del", KEY_DELETE},
	{"end", KEY_END},
	{"pgdn", KEY_PAGEDOWN},
	{"rightarrow", KEY_RIGHT},
	{"leftarrow", KEY_LEFT},
	{"downarrow", KEY_DOWN},
	{"uparrow", KEY_UP},
	{"numlock", KEY_NUMLOCK},
	{"kp_slash", KEY_KP_DIVIDE},
	{"kp_multiply", KEY_KP_MULTIPLY},
	{"kp_minus", KEY_KP_MINUS},
	{"kp_plus", KEY_KP_PLUS},
	{"kp_enter", KEY_KP_ENTER},
	{"kp_del", KEY_KP_PERIOD},
	{"kp_ins", KEY_KP0},
	{"kp_end", KEY_KP1},
	{"kp_downarrow", KEY_KP2},
	{"kp_pgdn", KEY_KP3},
	{"kp_leftarrow", KEY_KP4},
	{"kp_5", KEY_KP5},
	{"kp_rightarrow", KEY_KP6},
	{"kp_home", KEY_KP7},
	{"kp_uparrow", KEY_KP8},
	{"kp_pgup", KEY_KP9},
	{"ctrl", KEY_LCONTROL},
	{"shift", KEY_LSHIFT},
	{"alt", KEY_LALT},
	{"lgui", KEY_LGUI},
	{"rctrl", KEY_RCONTROL},
	{"rshift", KEY_RSHIFT},
	{"ralt", KEY_RALT},
	{"rgui", KEY_RGUI},
	{"app", KEY_APP},
	{"mouse1", MOUSE_LEFT},
	{"mouse2", MOUSE_RIGHT},
	{"mouse3", MOUSE_MIDDLE},
	{"mouse4", MOUSE_4},
	{"mouse5", MOUSE_5},
	{"mwheelup", MOUSE_WHEEL_UP},
	{"mwheeldown", MOUSE_WHEEL_DOWN}
};

static bool iequal(std::string_view sv1, std::string_view sv2)
{
	if(sv1.size() != sv2.size())
		return false;

	for(size_t i = 0; i < sv1.size(); i++)
	{
		if(std::tolower(sv1[i]) != std::tolower(sv2[i]))
			return false;
	}

	return true;
}

keycode_t string_to_keycode(std::string_view string)
{
	if(string.length() == 1)
	{
		char key = std::tolower(string[0]);
		if(key >= '0' && key <= '9')
			return keycode_t(KEY_0 + (key - '0'));
		if(key >= 'a' && key <= 'z')
			return keycode_t(KEY_A + (key - 'a'));
	}

	for(auto& entry : key_names)
	{
		if(iequal(string, entry.name))
			return entry.key;
	}

	return KEY_NONE;
}

constexpr const char* number_cstrs[] =
{
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

constexpr const char* alpha_cstrs[] =
{
	"a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
	"k", "l", "m", "n", "o", "p", "q", "r", "s", "t",
	"u", "v", "w", "x", "y", "z"
};

std::string_view keycode_to_string(keycode_t keycode)
{
	if(keycode >= KEY_0 && keycode <= KEY_9)
		return number_cstrs[keycode - KEY_0];
	else if(keycode >= KEY_A && keycode <= KEY_Z)
		return alpha_cstrs[keycode - KEY_A];

	for(auto& entry : key_names)
	{
		if(entry.key == keycode)
			return entry.name;
	}

	return "key_none";
}

}

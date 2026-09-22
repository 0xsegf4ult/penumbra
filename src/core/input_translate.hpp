#pragma once

#include <penumbra/input_keys.hpp>
#include <SDL3/SDL_scancode.h>

namespace penumbra
{

keycode_t sdl_scancode_parse(SDL_Scancode scancode);
SDL_Scancode keycode_to_sdl_key(keycode_t key);
keycode_t sdl_mouse_parse(u8 button);
keycode_t string_to_keycode(std::string_view string);
std::string_view keycode_to_string(keycode_t keycode);

}

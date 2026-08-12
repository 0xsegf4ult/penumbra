#pragma once

#include <penumbra/math/vector.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

void wm_init();
void wm_shutdown();

enum WM_MSGBOX_TYPE
{
	WM_MESSAGE_BOX_INFO,
	WM_MESSAGE_BOX_WARNING,
	WM_MESSAGE_BOX_ERROR
};

void wm_message_box(const char* title, const char* message, WM_MSGBOX_TYPE type);
using window_t = u64;

window_t wm_create_window(const char* title, uvec2 wnd_size);
void wm_destroy_window(window_t window);

void wm_poll_events();
bool wm_requested_close();

uvec2 wm_get_size(window_t window);
bool wm_is_fullscreen(window_t window);
void wm_set_fullscreen(window_t window, bool state);

}

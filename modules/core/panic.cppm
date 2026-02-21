export module penumbra.core:panic;

import :log;
import :window;

namespace penumbra
{

export [[noreturn]] void panic(const char* message)
{
	log::critical(message);
	wm_message_box("Fatal error", message, WM_MESSAGE_BOX_ERROR);
	std::terminate();		
}

}

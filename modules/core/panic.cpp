#include <penumbra/log.hpp>
#include <penumbra/window.hpp>

#include <exception>

namespace penumbra
{

[[noreturn]] void panic(const char* message)
{
	log::critical(message);
	wm_message_box("Fatal error", message, WM_MESSAGE_BOX_ERROR);
	std::terminate();		
}

}

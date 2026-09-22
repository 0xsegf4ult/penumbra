#include <penumbra/log.hpp>
#include <penumbra/window.hpp>

#include <string>
#include <exception>

namespace penumbra
{

[[noreturn]] void panic(std::string_view message)
{
	// string_view might point to non null terminated data 
	std::string msg{message};
	log::critical(message);
	wm_message_box("Fatal error", msg.c_str(), WM_MESSAGE_BOX_ERROR);
	std::terminate();		
}

}

#include <penumbra/cmd.hpp>
#include <penumbra/cvar.hpp>

#include <string_view>
#include <format>
#include <print>

namespace penumbra
{

void console_print(const char* text)
{
	std::print("{}", text);
}

static int cmd_tokenize(std::string_view cmd_text, std::string_view* argv)
{
	int argc = 0;

	auto start = cmd_text.find_first_not_of(" ");
	while(start != std::string_view::npos)
	{
		auto end = cmd_text.find_first_of(" ", start);
		if(argc == 16)
			return argc;

		argv[argc++] = cmd_text.substr(start, end - start);
		start = cmd_text.find_first_not_of(" ", end);
	}

	return argc;
}

void cmd_executestring(std::string_view cmd_text)
{
	std::string_view argv[16];
	int argc = cmd_tokenize(cmd_text, argv);
	if(argc == 0)
		return;
	
	cvar_t* cv = cvar_get(argv[0]); 
	if(cv)
	{
		if(argc == 1)
		{
			switch(cv->type)
			{
			case CVAR_TYPE_INT:
				console_print(std::format("{} is {}\n", cv->name, cv->int_v).c_str());
				break;
			case CVAR_TYPE_FLOAT:
				console_print(std::format("{} is {}\n", cv->name, cv->float_v).c_str());
				break;
			case CVAR_TYPE_STRING:
				console_print(std::format("{} is {}\n", cv->name, cv->string_v).c_str());
				break;
			}
		}
		else
		{
			cvar_set_string(cv, argv[1]);	
		}

		return;
	}

	console_print(std::format("Unknown command: {}\n", cmd_text).c_str());
}

}

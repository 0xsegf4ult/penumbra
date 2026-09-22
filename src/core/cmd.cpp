#include <penumbra/cmd.hpp>
#include <penumbra/cvar.hpp>

#include <string_view>
#include <format>
#include <print>

namespace penumbra
{

constexpr u32 cmd_max_sinks = 4;
static cmd_sink_callback cmd_sinks[cmd_max_sinks];
static cmd_t* cmdlist = nullptr;

void cmd_register(cmd_t* cmd)
{
	cmd->next = nullptr;

	if(cvar_get(cmd->name))
		return;

	if(cmd_get(cmd->name))
		return;

	if(!cmdlist || cmd->name < cmdlist->name)
	{
		cmd->next = cmdlist;
		cmdlist = cmd;
	}
	else
	{
		cmd_t* prev = cmdlist;
		cmd_t* cur = cmdlist->next;
		while(cur && cmd->name > cur->name)
		{
			prev = cur;
			cur = cur->next;
		}

		cmd->next = prev->next;
		prev->next = cmd;
	}
}

cmd_t* cmd_get(std::string_view name)
{
	cmd_t* cur = cmdlist;
	while(cur)
	{
		if(cur->name == name)
			return cur;
		
		cur = cur->next;
	}

	return nullptr;
}

void console_print(std::string_view text)
{
	std::print("{}", text);
	for(int i = 0; i < cmd_max_sinks; i++)
	{
		if(!cmd_sinks[i])
			return;

		cmd_sinks[i](text);
	}
}

void cmd_register_output_sink(cmd_sink_callback callback)
{
	for(int i = 0; i < cmd_max_sinks; i++)
	{
		if(!cmd_sinks[i])
		{
			cmd_sinks[i] = callback;
			return;
		}
	}
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

	cmd_t* cm = cmd_get(argv[0]);
	if(cm)
	{
		cm->callback({&argv[1], size_t(argc > 1 ? argc - 1 : 0u)});
		return;
	}

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

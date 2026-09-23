#include <penumbra/cmd.hpp>
#include <penumbra/cvar.hpp>
#include <penumbra/vfs.hpp>

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
	if(!cmd_text.empty() && cmd_text.back() == '\r')
		cmd_text.remove_suffix(1);

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

void cmd_executescript(std::string_view text)
{
	size_t pos = 0;
	while(pos <= text.size())
	{
		auto end = text.find('\n', pos);
		if(end == std::string_view::npos)
			end = text.size();

		auto line = text.substr(pos, end - pos);
		auto first = line.find_first_not_of(" \t");
		if(first == std::string_view::npos || line[first] != '#')
			cmd_executestring(line);

		pos = end + 1;
	}
}

constexpr int exec_max_depth = 16;
static int exec_depth = 0;

static void exec_cmd_cb(cmd_args_t args)
{
	if(args.size() < 1)
	{
		console_print("usage: exec <path>\n");
		return;
	}

	if(exec_depth >= exec_max_depth)
	{
		console_print(std::format("exec: recursion limit reached ({})\n", exec_max_depth));
		return;
	}

	vfs_fd file = vfs_open(vfs_path(args[0]), VFS_ACCESS_READ);
	if(file < 0)
	{
		console_print(std::format("exec: could not open {}\n", args[0]));
		return;
	}

	exec_depth++;
	if(auto size = vfs_size(file); size > 0)
		cmd_executescript({reinterpret_cast<const char*>(vfs_map(file)), size});
	exec_depth--;
	vfs_close(file);
}

static cmd_t exec_cmd
{
	.name = "exec",
	.callback = exec_cmd_cb
};

void cmd_init()
{
	cmd_register(&exec_cmd);
}


}

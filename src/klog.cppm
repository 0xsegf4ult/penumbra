export module klog;
import arch.x86_64;
import types;
import kfmt;

template <typename... Args>
void generic_log(const char* fmt_string, Args&&... args)
{
	static char buffer[512];
	format_to(string_span{&buffer[0], 512}, fmt_string, args...);
	
	early_serial_write(buffer);
	early_serial_putchar('\n');
}

export namespace log
{

template <typename... Args>
void debug(const char* fmt_string, Args&&... args)
{
	early_serial_write("\033[96m[debug]\033[0m ");
	generic_log(fmt_string, args...);
}

template <typename... Args>
void info(const char* fmt_string, Args&&... args)
{
	early_serial_write("\033[32m[info]\033[0m ");
	generic_log(fmt_string, args...);
}

template <typename... Args>
void warn(const char* fmt_string, Args&&... args)
{
	early_serial_write("\033[33m[warn]\033[0m ");
	generic_log(fmt_string, args...);
}

template <typename... Args>
void error(const char* fmt_string, Args&&... args)
{
	early_serial_write("\033[31m[error]\033[0m ");
	generic_log(fmt_string, args...);
}

}



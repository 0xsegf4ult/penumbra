export module kstd;
import kfmt;
import types;

export inline void* operator new(size_t, void* p) { return p; }
export inline void* operator new[](size_t, void* p) { return p; }
export inline void operator delete(void*, void*) {};
export inline void operator delete[](void*, void*) {};

void panic_inner(const char* string);

export template <typename... Args>
void panic(const char* fmt_string, Args&&... args)
{
	static char buffer[512];
	format_to(string_span{&buffer[0], 512}, fmt_string, args...);
	panic_inner(buffer);
}

export void panic(const char* string)
{
	panic_inner(string);
}

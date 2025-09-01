module kstd;
import types;
import arch.x86_64;

void panic_inner(const char* string)
{
	early_serial_write("\033[31mkernel panic:\033[0m ");
	early_serial_write(string);
	early_serial_putchar('\n');
	CPU::halt();
}

extern "C" void* memcpy(void* dest, const void* src, size_t n)
{
	uint8_t* pdest = reinterpret_cast<uint8_t*>(dest);
	const uint8_t* psrc = reinterpret_cast<const uint8_t*>(src);

	for(size_t i = 0; i < n; i++)
	{
		pdest[i] = psrc[i];
	}

	return dest;
}

extern "C" void* memset(void* dest, int data, size_t n)
{
	uint8_t* pdest = reinterpret_cast<uint8_t*>(dest);

	for(size_t i = 0; i < n; i++)
	{
		pdest[i] = static_cast<uint8_t>(data);
	}

	return pdest;
}

extern "C" void* memmove(void* dest, const void* src, size_t n)
{
	uint8_t* pdest = reinterpret_cast<uint8_t*>(dest);
	const uint8_t* psrc = reinterpret_cast<const uint8_t*>(src);

	if(src > dest)
	{
		for(size_t i = 0; i < n; i++)
			pdest[i] = psrc[i];
	}
	else if(src < dest)
	{
		for(size_t i = 0; i < n; i++)
			pdest[i - 1] = psrc[i - 1];
	}

	return dest;
}

extern "C" int memcmp(const void* s1, const void* s2, size_t n)
{
	const uint8_t* p1 = reinterpret_cast<const uint8_t*>(s1);
	const uint8_t* p2 = reinterpret_cast<const uint8_t*>(s2);

	for(size_t i = 0; i < n; i++)
	{
		if(p1[i] != p2[i])
			return p1[i] < p2[i] ? -1 : 1;
	}

	return 0;
}

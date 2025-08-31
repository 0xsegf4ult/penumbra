export module arch.x86_64:io;
import types;

export namespace io
{
	void outb(uint8_t value, uint16_t port)
	{
		asm volatile("outb %b0, %w1" : : "a" (value), "dN" (port));
	}

	uint8_t inb(uint16_t port)
	{
		uint8_t value;
		asm volatile("inb %w1, %0" : "=a" (value) : "dN" (port));
		return value;
	}

	void outw(uint16_t value, uint16_t port)
	{
		asm volatile("outw %w0, %w1" : : "a" (value), "dN" (port));
	}

	uint16_t inw(uint16_t port)
	{
		uint16_t value;
		asm volatile("inw %w1, %0" : "=a" (value) : "dN" (port));
		return value;
	}

	void outl(uint32_t value, uint16_t port)
	{
		asm volatile("outl %0, %w1" : : "a" (value), "dN" (port));
	}

	uint32_t inl(uint16_t port)
	{
		uint32_t value;
		asm volatile("inl %w1, %0" : "=a" (value) : "dN" (port));
		return value;
	}
}

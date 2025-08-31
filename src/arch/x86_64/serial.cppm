export module arch.x86_64:serial;
import :io;

import types;

export namespace serial
{
	enum UART
	{
		DLL = 0,
		IER = 1,
		DLH = 1,
		FCR = 2,
		LCR = 3,
		MCR = 4,
		LSR = 5
	};

	constexpr uint16_t COM1 = 0x3f8;
}
	
using namespace serial;
export void early_serial_init()
{
	io::outb(0x00, COM1 + UART::IER);
	io::outb(0x80, COM1 + UART::LCR);
	io::outb(0x03, COM1 + UART::DLL);
	io::outb(0x00, COM1 + UART::DLH);
	io::outb(0x03, COM1 + UART::LCR);
	io::outb(0xc7, COM1 + UART::FCR);
	io::outb(0x0b, COM1 + UART::MCR);
}

export uint8_t early_serial_tx_empty()
{
	return io::inb(COM1 + UART::LSR) & 0x20;
}

export void early_serial_putchar(char chr)
{
	while(!early_serial_tx_empty());

	io::outb(chr, COM1);
}

export void early_serial_write(const char* string)
{
	size_t chr = 0;
	while(string[chr] != '\0')
	{
		early_serial_putchar(string[chr]);
		chr++;
	}	
}

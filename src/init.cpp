#include <limine.h>

import arch.x86_64;
import types;

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

extern "C" [[noreturn]] void init()
{
	early_serial_init();
	early_serial_write("[cpu0] penumbra kernel v0.1\n");

	CPU::halt();
}

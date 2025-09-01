#include <limine.h>

import arch.x86_64;
import mm;
import klog;
import types;

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request =
{
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_stack_size_request ss_request =
{
	.id = LIMINE_STACK_SIZE_REQUEST,
	.stack_size = 8192
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

extern "C" char* virt_kernel_start;
extern "C" char* virt_kernel_end;

typedef void (*ctor_func_t)();
extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];

extern "C" void __assertion_fail_handler(const char* str)
{
	early_serial_write("assertion failed: ");
	early_serial_write(str);
	early_serial_putchar('\n');
	CPU::halt();
}

extern "C" [[noreturn]] void init()
{
	CPU bootCPU;
	
	for(ctor_func_t* ctor = start_ctors; ctor < end_ctors; ctor++)
		(*ctor)();

	early_serial_init();
	bootCPU.early_init(0);
	
	log::info("penumbra kernel version git-");

	log::info("kernel virt memory [{} - {}]", reinterpret_cast<void*>(&virt_kernel_start), reinterpret_cast<void*>(&virt_kernel_end));
	auto mmap = mm::parse_memmap(memmap_request.response->entries, memmap_request.response->entry_count);
	mm::initialize(mmap);

	CPU::halt();
}

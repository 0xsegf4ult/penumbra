export module arch.x86_64:interrupts;
import types;
import kstd;
import :serial;
import :cpu;

struct __attribute__((packed)) ISRStack
{
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;

	uint64_t intr_id;
	uint64_t intr_res;

	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

enum InterruptID
{
	DivisionError,
	Debug,
	NMI,
	Breakpoint,
	Overflow,
	BoundExceeded,
	InvalidOpcode,
	DeviceUnavailable,
	DoubleFault,
	CSegOverrun,
	InvalidTSS,
	SegmentNotPresent,
	StackSegmentFault,
	GPFault,
	PageFault,
	Reserved,
	FPUException,
	AlignmentCheck,
	MachineCheck,
	SIMDFPException,
	VirtualizationException,
	ControlProtectionException,
	HypervisorInj = 28,
	VMMCommunication = 29,
	SecurityException = 30,
};

extern "C" uint64_t interrupt_handler(ISRStack* stack)
{
	if(stack->intr_id == InterruptID::PageFault)
	{
		uint64_t cr2;
		asm volatile("movq %%cr2, %0" : "=r"(cr2));

		panic("unhandled page fault at RIP {} memory access {}", reinterpret_cast<void*>(stack->rip), reinterpret_cast<void*>(cr2));
	}

	panic("unhandled interrupt");
	return stack->rsp;
}

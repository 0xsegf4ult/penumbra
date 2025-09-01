export module arch.x86_64:cpu;
import :paging;
import types;

export enum MSRRegisters
{
	FS_BASE = 0xc0000100,
	GS_BASE = 0xc0000101,
	KERNEL_GS_BASE = 0xc0000102
};

export enum Selector
{
	KERNEL_CS = (1 << 3),
	KERNEL_DS = (2 << 3)
};

export enum GDTFlags
{
	accessed = 0b1,
	writable = 0b10,
	readable = 0b10,
	executable = 0b1000,
	CS = 0b10000,
	DS = 0b10000,
	present = 0b10000000
};

export enum DPL
{
	kernel = 0x0,
	user = 0x3
};

export struct __attribute__((packed)) GDTEntry
{
	GDTEntry()
	{
		limit_low = 0;
		base_low = 0;
		base_mid = 0;
		flags = 0;
		granularity = 0;
		base_high = 0;
	}
	
	GDTEntry(uint8_t flags_sel, uint8_t priv)
	{
		limit_low = 0;
		base_low = 0;
		base_mid = 0;
		flags = flags_sel | (priv << 6) | GDTFlags::present | GDTFlags::accessed;
		granularity = 0b10100000;
		base_high = 0;
	}

	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;

	uint8_t flags;
	uint8_t granularity;

	uint8_t base_high;
};
static_assert(sizeof(GDTEntry) == sizeof(uint64_t));

export struct __attribute__((packed)) GDTDescriptor
{
	uint16_t limit;
	GDTEntry* base;
};

export enum IDTType
{
	intr_gate = 0x8e,
	trap_gate = 0x8f
};

export struct __attribute__((packed)) IDTEntry
{
	IDTEntry() = default;

	IDTEntry(uint64_t base, uint16_t sel, uint8_t gate_type, uint8_t priv)
	{
		base16_low = base & 0xFFFF;
		base16_high = (base >> 16) & 0xFFFF;
		base32_high = base >> 32;
		selector = sel;
		type = gate_type | (priv << 5);
		zero = 0;
		padding = 0;
	}

	uint16_t base16_low;
	uint16_t selector;

	uint8_t zero;
	uint8_t type;

	uint16_t base16_high;
	uint32_t base32_high;
	uint32_t padding;
};
static_assert(sizeof(IDTEntry) == 16);

export struct __attribute__((packed)) IDTDescriptor
{
	uint16_t limit;
	IDTEntry* base;
};

extern "C" void reload_segments();
extern "C" void isr_stubs();

export class CPU
{
public:
	CPU() = default;

	void early_init(uint32_t cpu)
	{
		disable_interrupts();
		self = this;
		id = cpu;

		init_gdt();
		
		wrmsr(MSRRegisters::GS_BASE, reinterpret_cast<virtaddr_t>(this));
		virtaddr_t isr_start = reinterpret_cast<virtaddr_t>(&isr_stubs);

		for(int i = 0; i < 32; i++)
			idt_entries[i] = IDTEntry(isr_start + (i * 16), Selector::KERNEL_CS, IDTType::trap_gate, DPL::kernel);

		for(int i = 32; i < 256; i++)
			idt_entries[i] = IDTEntry(isr_start + (i * 16), Selector::KERNEL_CS, IDTType::intr_gate, DPL::kernel);

		idtr.limit = sizeof(IDTEntry) * 256 - 1;
		idtr.base = this->idt_entries;

		asm volatile("lidt %0" : : "m"(this->idtr) : "memory");
		enable_interrupts();
	}
	
	void set_pagetable(PageTable* pt_address)
	{
		page_table = pt_address;
		asm volatile("movq %0, %%cr3" : : "a" (reinterpret_cast<uint64_t>(pt_address)));
	}

	static uint64_t rdsmr(uint64_t msr)
	{
		uint32_t low, high;
		asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
		return (static_cast<uint64_t>(high) << 32) | low;
	}

	static void wrmsr(uint64_t msr, uint64_t value)
	{
		const uint32_t low = value & 0xFFFFFFFF;
		const uint32_t high = value >> 32;
	       	asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
	}

	static uint64_t read_gs_ptr(uint64_t offset)
	{
		uint64_t value;
		asm volatile("mov %%gs:%a[off], %[val]" : [val] "=r"(value) : [off] "ir"(offset));
		return value;
	}

	static CPU* get_current()
	{
		return reinterpret_cast<CPU*>(CPU::read_gs_ptr(__builtin_offsetof(CPU, self)));
	}

	static uint32_t get_current_id()
	{
		return CPU::read_gs_ptr(__builtin_offsetof(CPU, id));
	}
	
	static void disable_interrupts()
	{
		asm volatile("cli");
	}

	static void enable_interrupts()
	{
		asm volatile("sti");
	}

	[[noreturn]] static void halt()
	{
		for(;;)
		{
			asm volatile("cli; hlt");
		}
	}

private:
	CPU* self;
	uint32_t id;

	GDTDescriptor gdtr;
	alignas(16) GDTEntry gdt_entries[16];

	IDTDescriptor idtr;
	alignas(16) IDTEntry idt_entries[256];
	
	PageTable* page_table;

	void init_gdt()
	{
		gdt_entries[0] = GDTEntry();
		gdt_entries[1] = GDTEntry(GDTFlags::CS | GDTFlags::executable | GDTFlags::readable, DPL::kernel);
		gdt_entries[2] = GDTEntry(GDTFlags::DS | GDTFlags::writable, DPL::kernel);
		gdt_entries[3] = GDTEntry(GDTFlags::CS | GDTFlags::executable | GDTFlags::readable, DPL::user);
		gdt_entries[4] = GDTEntry(GDTFlags::DS | GDTFlags::writable, DPL::user);

		gdtr.limit = 5 * sizeof(GDTEntry) - 1;
		gdtr.base = this->gdt_entries;

		asm volatile("lgdt %0" :: "m"(this->gdtr) : "memory");
		reload_segments();
	}
};

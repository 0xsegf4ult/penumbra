export module arch.x86_64:paging;
import types;

export namespace mem
{

enum PageTableBits
{
	present = (1 << 0),
	writable = (1 << 1),
	user = (1 << 2),
	writethrough = (1 << 3),
	cache_disable = (1 << 4),
	accessed = (1 << 5),
	dirty = (1 << 6),
	huge = (1 << 7),
	global = (1 << 8),
	noexec = 0x8000000000000000
};

constexpr uint64_t page_mask_4K = 0xfff0fffffffff000;
constexpr uint64_t page_mask_2M = 0xfff0ffffffe00000;
constexpr uint64_t page_mask_1G = 0xfff0ffffe0000000;

enum PageSize
{
	GB = (1 << 30),
	MB = (1 << 21),
	KB = (1 << 12)
};

constexpr uint64_t get_pagetable_index(uint64_t address, int8_t level = 1)
{
	int8_t shift = 12 + ((level - 1) * 9);
	return (address & (0x1ffzu << shift)) >> shift;
}

constexpr size_t arch_page_size = 4096;

constexpr uint64_t page_align(uint64_t address)
{
	return (address + PageSize::KB - 1) & (~(PageSize::KB - 1));
}

constexpr uint64_t page_align_down(uint64_t address)
{
	return address & ~(PageSize::KB - 1);
}

constexpr uint32_t addr_to_pagenum(uint64_t address)
{
	return address >> 12;
}

constexpr uint64_t pagenum_to_addr(uint32_t pagenum)
{
	return pagenum << 12;
}

constexpr uint64_t kernel_mapping_offset = 0xffffffff80000000;
constexpr uint64_t direct_mapping_offset = 0xffff800000000000;

}

export class PageTable
{
public:
	constexpr PageTable() : raw{0} {}
	constexpr PageTable(uint64_t value) : raw{value} {}
	constexpr uint64_t get_raw() const
	{
		return raw;
	}

	constexpr uint64_t get() const
	{
		return raw & 0x000ffffffffff000;
	}

	constexpr void set_flags(uint64_t flags)
	{
		raw |= flags;
	}

	constexpr void unset_flags(uint64_t flags)
	{
		raw &= ~flags;
	}
private:
	uint64_t raw;
};

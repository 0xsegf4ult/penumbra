module;

#include <limine.h>

export module mm:memory_map;
import arch.x86_64;
import kstd;
import klog;
import types;

constexpr const char* region_type_strings[4] =
{
	"usable",
	"kernel",
	"reserved",
	"allocated"
};

export namespace mm
{

struct mem_region
{
	uint64_t start;
	uint64_t end;
	
	enum class RegionType
	{
		Usable,
		Kernel,
		Reserved,
		Allocated
	} type;
};

struct MemoryMap
{
	constexpr static size_t max_regions = 64;

	mem_region regions[max_regions];
	uint32_t num_regions{0};
	size_t memory_top{0};

	void* reserve(size_t size)
	{
		for(mem_region* region = regions; region != regions + num_regions; region++)
		{
			if(region->type != mem_region::RegionType::Usable)
				continue;

			uint64_t start = mem::page_align(region->start);
			uint64_t end = mem::page_align_down(region->end);

			if(start < end && end - start >= size)
			{
				if(num_regions == max_regions)
					return nullptr;
				
				region->start = start + size;

				mem_region* new_region = regions + num_regions;
	
				new_region->start = start;
				new_region->end = mem::page_align(start + size);
				new_region->type = mem_region::RegionType::Allocated;

				num_regions++;
				return reinterpret_cast<void*>(start + mem::direct_mapping_offset);
			}
		}

		return nullptr;
	}
};

MemoryMap parse_memmap(limine_memmap_entry** entries, size_t entry_count)
{
	MemoryMap mmap;

	if(entry_count > MemoryMap::max_regions)
	{
		panic("failed to parse memory map, too many entries");
	}

	for(limine_memmap_entry* entry = entries[0]; entry < entries[0] + entry_count; entry++)
	{
		auto& region = mmap.regions[mmap.num_regions];
		
		region.start = entry->base;
		region.end = entry->base + entry->length;
		
		switch(entry->type)
		{
		case LIMINE_MEMMAP_USABLE:
			if(region.end > mmap.memory_top)
				mmap.memory_top = region.end;

			region.type = mem_region::RegionType::Usable;
			break;
		case LIMINE_MEMMAP_RESERVED:
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
		case LIMINE_MEMMAP_ACPI_NVS:
		case LIMINE_MEMMAP_BAD_MEMORY:
		default:
			region.type = mem_region::RegionType::Reserved;
			break;
		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
		case LIMINE_MEMMAP_FRAMEBUFFER:
			region.type = mem_region::RegionType::Allocated;
			break;
		}

		log::info("memmap: [mem {} - {}] {}", reinterpret_cast<void*>(region.start), reinterpret_cast<void*>(region.end), region_type_strings[static_cast<int>(region.type)]);

		mmap.num_regions++;
	}

	return mmap;
}

}

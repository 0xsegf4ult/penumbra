export module mm;
export import :memory_map;
import kstd;
import arch.x86_64;

struct mm_context_t
{
	mm::MemoryMap memory_map;
};

mm_context_t* mm_context = nullptr;

namespace mm
{

export void initialize(MemoryMap& memmap)
{
	auto mm_block = memmap.reserve(sizeof(mm_context_t));
	if(mm_block == nullptr)
	{
		panic("failed to reserve memory for mm_context");
	}

	mm_context = new (mm_block) mm_context_t();
	
	mm_context->memory_map = memmap;
}

}

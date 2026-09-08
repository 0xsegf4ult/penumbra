#include <penumbra/memory.hpp>
#include <penumbra/types.hpp>
#include <cstdlib>
#include <new>
#include <tracy/Tracy.hpp>

namespace penumbra
{

const char* tracypool_name = "heap_generic";

void* heap_allocate(size_t size)
{
	void* ptr = std::malloc(size);
	TracyAllocN(ptr, size, tracypool_name);
	return ptr;
}

void heap_free(void* ptr)
{
	TracyFreeN(ptr, tracypool_name);
	std::free(ptr);
}

}

void* operator new(size_t size)
{
	void* ptr = penumbra::heap_allocate(size);
	return ptr;
}

void operator delete(void* ptr) noexcept
{
	penumbra::heap_free(ptr);
}

void* operator new[](size_t size)
{
	void* ptr = penumbra::heap_allocate(size);
	return ptr;
}

void operator delete[](void* ptr) noexcept
{
	penumbra::heap_free(ptr);
}

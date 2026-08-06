module;

#if defined __linux__
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#elif defined _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>
#endif

export module penumbra.core:pool_allocator;

import std;
import :log;

using std::size_t, std::uint32_t, std::uint64_t;

namespace penumbra
{

export template <typename T>
class pool_allocator
{
public:
	constexpr static uint32_t invalid_object = 0xFFFFFFFF;
	
	struct internal_object
	{
		T data;
		std::atomic<uint32_t> next{invalid_object};
	};
	static_assert(alignof(internal_object) == alignof(T));

	pool_allocator(std::string_view unique_name) : name{unique_name} {}
	~pool_allocator()
	{
		#if defined __linux__
		munmap(objects, map_length);
		#elif defined _WIN32
		VirtualFree(objects, 0ul, MEM_RELEASE);
		#endif
	}

	pool_allocator(const pool_allocator&) = delete;
	pool_allocator(pool_allocator&&) = delete;

	pool_allocator& operator=(const pool_allocator&) = delete;
	pool_allocator& operator=(pool_allocator&&) = delete;

	void init(uint32_t max_obj, uint32_t prefault = 1)
	{
		max_objects = max_obj;
		map_length = max_objects * sizeof(internal_object);
		const uint32_t pf_size = ((prefault * sizeof(internal_object)) + 4096u) / 4096u;
		[[maybe_unused]] const auto pf_objects = std::min<uint32_t>(pf_size * 4096u / sizeof(internal_object), max_objects);

		#if defined __linux__
		objects = (internal_object*)mmap(nullptr, map_length, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
		if(objects == MAP_FAILED)
			log::critical("pool_allocator[{}]: mmap failed with error {}", name, std::strerror(errno));
		#elif defined _WIN32
		objects = (internal_object*)VirtualAlloc(nullptr, map_length, MEM_RESERVED, PAGE_NOACCESS);
		VirtualAlloc(objects, prefault * sizeof(internal_object), MEM_COMMIT, PAGE_READWRITE);
		num_allocations = pf_objects;
		#endif

		monotonic_ctr = 0u;
		xchg_aba_tag = 0u;
		freelist_head = invalid_object;
	}

	template <typename... Args>
	uint32_t allocate(Args&&... args) noexcept
	{
		for(;;)
		{
			uint64_t tag_fh = freelist_head.load(std::memory_order_acquire);
			uint32_t fhead = tag_fh & 0xFFFFFFFF;
			if(fhead == invalid_object)
			{
				fhead = monotonic_ctr.fetch_add(1, std::memory_order_relaxed);
				if(fhead >= max_objects)
				{
					log::critical("pool_allocator[{}]: out of memory", name);
					return invalid_object;
				}

				return internal_allocate(fhead, std::forward<Args>(args)...);
			}

			uint32_t next = get_internal(fhead).next.load(std::memory_order_acquire);
			uint64_t new_freelist_head = (uint64_t(xchg_aba_tag.fetch_add(1, std::memory_order_relaxed)) << 32u) | next;

			if(freelist_head.compare_exchange_weak(tag_fh, new_freelist_head, std::memory_order_release))
				return internal_allocate(fhead, std::forward<Args>(args)...);
		}
	}

	void free(uint32_t handle) noexcept
	{
		internal_object& obj = get_internal(handle);
		obj.data.~T();

		for(;;)
		{
			uint64_t tag_fh = freelist_head.load(std::memory_order_acquire);
			uint32_t fhead = tag_fh & 0xFFFFFFFF;

			obj.next.store(fhead, std::memory_order_release);

			uint64_t new_freelist_head = (static_cast<uint64_t>(xchg_aba_tag.fetch_add(1u, std::memory_order_relaxed)) << 32u) | handle;
			if(freelist_head.compare_exchange_weak(tag_fh, new_freelist_head, std::memory_order_release))
				return;
		}
	}

	constexpr T& get(uint32_t handle) noexcept
	{
		return objects[handle].data;
	}

	constexpr const T& get(uint32_t handle) const noexcept
	{
		return objects[handle].data;
	}
private:
	constexpr internal_object& get_internal(uint32_t handle) noexcept
	{
		return objects[handle];
	}

	template <typename... Args>
	uint32_t internal_allocate(uint32_t index, Args&&... args) noexcept
	{
		#if defined _WIN32
		num_allocations = std::max(num_allocations.load(), index + 1);
		VirtualAlloc(objects, sizeof(internal_object) * num_allocations, MEM_COMMIT, PAGE_READWRITE);
		#endif

		internal_object& obj = get_internal(index);
		::new (&obj.data) T(std::forward<Args>(args)...);
		obj.next.store(index, std::memory_order_release);
		return index;
	}

	std::string_view name;

	internal_object* objects;
	uint32_t max_objects;
	size_t map_length;

	#if defined _WIN32
	std::atomic<uint32_t num_allocations;
	#endif

	std::atomic<uint32_t> monotonic_ctr;
	std::atomic<uint32_t> xchg_aba_tag;
	std::atomic<uint64_t> freelist_head;
};

}

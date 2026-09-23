#include <penumbra/vfs.hpp>
#include <penumbra/panic.hpp>
#include <penumbra/log.hpp>
#include <penumbra/types.hpp>

#include <bit>
#include <expected>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <vector>

#if defined __linux__
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#elif defined _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace penumbra
{

struct file_t
{
	#if defined __linux__
	int fd;
	#elif defined _WIN32
	HANDLE fd;
	HANDLE map;
	#endif
	size_t size;
	u8* mapped;
	bool rw;
};

struct vfs_context_t
{
	std::vector<file_t> table;
	std::vector<u64> bitmap;
	std::shared_mutex lock;
};
static vfs_context_t* context = nullptr;

void vfs_init()
{
	context = new vfs_context_t();

	#if defined __linux__
	struct rlimit lim;
	getrlimit(RLIMIT_NOFILE, &lim);
	lim.rlim_cur = 65536;
	setrlimit(RLIMIT_NOFILE, &lim);
	#endif

	context->table.resize(65536);
	context->bitmap.resize(65536 / 64);

	for(size_t i = 0; i < context->bitmap.size(); i++)
		context->bitmap[i] = ~(0ull);
}

void vfs_shutdown()
{
	delete context;
}

static vfs_fd get_free_fd()
{
	std::unique_lock<std::shared_mutex> r_lock{context->lock};

	for(size_t i = 0; i < context->bitmap.size(); i++)
	{
		u64 cur_word = context->bitmap[i];
		if(cur_word == 0)
			continue;

		int index = __builtin_ctzll(cur_word);
		context->bitmap[i] &= ~(1ull << index);
		return (i * 64 + index);
	}

	log::error("vfs: out of file descriptors");
	return -1;
}

static void release_fd(vfs_fd fd)
{
	std::unique_lock<std::shared_mutex> w_lock{context->lock};

	auto bmp_offset = fd / 64;
	auto bit_offset = fd % 64;
	context->bitmap[bmp_offset] |= (1ull << bit_offset);
}

vfs_fd vfs_open(const vfs_path& p, vfs_access_t mode)
{
	vfs_fd handle = get_free_fd();
	if(handle < 0)
		return handle;

	file_t& f = context->table[handle];
	f.size = 0;
	f.mapped = nullptr;
	#if defined _WIN32
	f.map = 0;
	#endif

	auto fail = [&]() -> vfs_fd
	{
		release_fd(handle);
		return -1;
	};

	if(!std::filesystem::exists(p))
		return fail();

	if(std::filesystem::is_directory(p))
		return fail();

	#if defined __linux__

	mode_t native_mode = 0;
	int prot = PROT_READ;
	switch(mode)
	{
	case VFS_ACCESS_READ:
		native_mode = O_RDONLY;
		f.rw = false;
		break;
	case VFS_ACCESS_RW:
		native_mode = O_RDWR;
		prot |= PROT_WRITE;
		f.rw = true;
		break;
	}

	f.fd = open(p.c_str(), native_mode);
	if(f.fd < 0)
	{
		std::perror("vfs_open: open() failed");
		return fail();
	}

	struct stat file_info;
	if(fstat(f.fd, &file_info) < 0)
	{
		std::perror("vfs_open: stat() failed");
		close(f.fd);
		return fail();
	}
	f.size = static_cast<size_t>(file_info.st_size);

	if(f.size > 0)
	{
		f.mapped = reinterpret_cast<u8*>(mmap(nullptr, f.size, prot, MAP_PRIVATE, f.fd, 0));
		if(f.mapped == MAP_FAILED)
		{
			std::perror("vfs_open: mmap() failed");
			f.mapped = nullptr;
			close(f.fd);
			return fail();
		}
	}
	#elif defined _WIN32

	if(mode != VFS_ACCESS_READ)
		panic("vfs_open: unimplemented access mode");

	f.rw = false;

	f.fd = CreateFileW
	(
		p.c_str(),
		GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0
	);

	if(f.fd == INVALID_HANDLE_VALUE)
	{
		log::error("vfs_open: CreateFileW failed: {}", GetLastError());
		return fail();
	}

	LARGE_INTEGER fsize;
	if(!GetFileSizeEx(f.fd, &fsize))
	{
		log::error("vfs_open: GetFileSizeEx failed: {}", GetLastError());
		CloseHandle(f.fd);
		return fail();
	}

	f.size = static_cast<size_t>(fsize.QuadPart);

	if(f.size > 0)
	{
		f.map = CreateFileMapping
		(
			f.fd,
			nullptr,
			PAGE_READONLY,
			0,
			0,
			nullptr
		);

		if(f.map == 0)
		{
			log::error("vfs_open: CreateFileMapping failed: {}", GetLastError());
			CloseHandle(f.fd);
			return fail();
		}

		f.mapped = reinterpret_cast<u8*>(MapViewOfFile
		(
			f.map,
			FILE_MAP_READ,
			0, 0, 0
		));

		if(f.mapped == nullptr)
		{
			log::error("vfs_open: MapViewOfFile failed: {}", GetLastError());
			CloseHandle(f.map);
			CloseHandle(f.fd);
			return fail();
		}
	}
	#else
	static_assert(false, "vfs_open not implemented");
	#endif

	return handle;
}

void vfs_close(vfs_fd fd)
{
	file_t& f = context->table[fd];
	#if defined __linux__
	if(f.mapped)
		munmap(f.mapped, f.size);
	close(f.fd);
	#elif defined _WIN32
	if(f.mapped)
		UnmapViewOfFile(f.mapped);
	if(f.map)
		CloseHandle(f.map);
	CloseHandle(f.fd);
	#else
	static_assert(false, "vfs_close not implemented");
	#endif

	release_fd(fd);
}

const u8* vfs_map(vfs_fd fd)
{
	std::scoped_lock<std::shared_mutex> r_lock{context->lock};
	return std::bit_cast<const u8*>(context->table[fd].mapped);
}

u8* vfs_map_rw(vfs_fd fd)
{
	std::scoped_lock<std::shared_mutex> r_lock{context->lock};
	return context->table[fd].mapped;
}

size_t vfs_size(vfs_fd fd)
{
	std::shared_lock<std::shared_mutex> r_lock{context->lock};
	return context->table[fd].size;
}

}

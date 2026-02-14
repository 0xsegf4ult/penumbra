module;

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

export module penumbra.core:vfs;
import :hash;
import :log;
import :strongly_typed;
import std;

using std::uint32_t;

namespace penumbra::vfs
{

export using std::filesystem::path;

export struct access_readonly_t{};
export constexpr access_readonly_t access_readonly;

export struct access_rw_t{};
export constexpr access_rw_t access_rw;

export enum class file_open_error
{
	unknown,
	no_entry,
	is_directory
};

export std::string_view file_open_error_to_string(file_open_error e)
{
	switch(e)
	{
	using enum file_open_error;
	case unknown:
	return "unknown error";
	case no_entry:
	return "no such file";
	case is_directory:
	return "is a directory";
	}
}

export struct file_t
{
	#if defined __linux__ 
	int fd;
	#elif defined _WIN32
	HANDLE fd;
	HANDLE map;
	#endif
	std::size_t size;
	void* mapped;
	bool rw;
};

export using file_handle = strongly_typed<uint32_t, struct _file_handle_tag>;

struct vfs_context_t
{
	std::unordered_map<file_handle, file_t> open_files;
	std::shared_mutex lock;
};
vfs_context_t* context = nullptr; 

using open_return_type = std::expected<file_handle, file_open_error>;
export open_return_type open_unscoped(const path& p, access_readonly_t)
{
	file_handle fh{fnv::hash(p.c_str())};
	{
	std::scoped_lock<std::shared_mutex> r_lock{context->lock};

	if(context->open_files.contains(fh))
		return fh;

	}

	if(!std::filesystem::exists(p))
		return std::unexpected(file_open_error::no_entry);

	if(std::filesystem::is_directory(p))
		return std::unexpected(file_open_error::is_directory);

	file_t f{};
	#if defined __linux__ 
	f.fd = ::open(p.c_str(), O_RDONLY);
	if(f.fd < 0)
	{
		std::perror("failed to open file");
		return std::unexpected(file_open_error::unknown);
	}
	struct stat file_info;
	if(fstat(f.fd, &file_info) < 0)
	{
		std::perror("failed to stat file");
		return std::unexpected(file_open_error::unknown);
	}
	f.size = static_cast<std::size_t>(file_info.st_size);

	f.mapped = mmap(nullptr, f.size, PROT_READ, MAP_PRIVATE, f.fd, 0);
	if(f.mapped == MAP_FAILED)
	{
		std::perror("failed to mmap file");
		return std::unexpected(file_open_error::unknown);
	}
	#elif defined _WIN32
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
		log::error("CreateFileW failed with error {}", GetLastError());
		return std::unexpected(file_open_error::unknown);
	}

	LARGE_INTEGER fsize;
	if(!GetFileSizeEx(f.fd, &fsize))
	{
		log::error("GetFileSizeEx failed with error {}", GetLastError());
		CloseHandle(f.fd);
		return std::unexpected(file_open_error::unknown);
	}

	f.size = static_cast<size_t>(fsize.QuadPart);

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
		log::error("CreateFileMapping failed with error {}", GetLastError());
		CloseHandle(f.fd);
		return std::unexpected(file_open_error::unknown);
	}

	f.mapped = MapViewOfFile
	(
		f.map,
		FILE_MAP_READ,
		0, 0, 0
	);

	if(f.mapped == nullptr)
	{
		log::error("MapViewOfFile failed with error {}", GetLastError());
		CloseHandle(f.map);
		CloseHandle(f.fd);
		return std::unexpected(file_open_error::unknown);
	}
	#else
	static_assert(false, "not implemented for current platform");
	#endif
	
	f.rw = false;
	
	{
	std::unique_lock<std::shared_mutex> w_lock{context->lock};
	context->open_files[fh] = f;
	}

	return fh;
}

export open_return_type open_unscoped(const path& p, access_rw_t)
{
	file_handle fh{fnv::hash(p.c_str())};
	
	{
	std::scoped_lock<std::shared_mutex> r_lock{context->lock};
	
	if(context->open_files.contains(fh))
	{
		if(!context->open_files[fh].rw) [[unlikely]]
		{
			log::critical("Tried to reopen readonly file as rw");
			std::unreachable();
		}
		return fh;
	}

	}

	if(std::filesystem::exists(p))
		return std::unexpected(file_open_error::no_entry);

	if(std::filesystem::is_directory(p))
		return std::unexpected(file_open_error::is_directory);

	file_t f{};
	#if defined __linux__
	f.fd = ::open(p.c_str(), O_RDWR);
	if(f.fd < 0)
		return std::unexpected(file_open_error::unknown);

	struct stat file_info;
	if(fstat(f.fd, &file_info) < 0)
		return std::unexpected(file_open_error::unknown);
	f.size = file_info.st_size;

	f.mapped = mmap(nullptr, f.size, PROT_READ | PROT_WRITE, MAP_PRIVATE, f.fd, 0);
	if(f.mapped == MAP_FAILED)
		return std::unexpected(file_open_error::unknown);
	#elif defined _WIN32
	log::critical("vfs_win32: open_rw STUBBED");
	#else
	static_assert(false, "not implemented for current platform");
	#endif

	f.rw = true;
	
	{
	std::unique_lock<std::shared_mutex> w_lock{context->lock};
	context->open_files[fh] = f;
	}

	return fh;
}

export void close(file_handle h)
{
	{
	std::scoped_lock<std::shared_mutex> r_lock{context->lock};
	const file_t& f = context->open_files[h];
	#if defined __linux__ 
	munmap(f.mapped, f.size);
	::close(f.fd);
	#elif defined _WIN32 
	UnmapViewOfFile(f.mapped);
	CloseHandle(f.map);
	CloseHandle(f.fd);
	#else
	static_assert(false, "not implemented for current platform");
	#endif
	}

	std::unique_lock<std::shared_mutex> w_lock{context->lock};
	context->open_files.erase(h);
}

struct ScopedFileHandle : public open_return_type
{
	~ScopedFileHandle()
	{
		if(has_value())
			vfs::close(this->operator*());
	}
};

export ScopedFileHandle open(const path& p, access_readonly_t ro_tag)
{
	return {open_unscoped(p, ro_tag)};
}

export ScopedFileHandle open(const path& p, access_rw_t rw_tag)
{
	return {open_unscoped(p, rw_tag)};
}

export template <typename T>
const T* map(file_handle h, access_readonly_t)
{
	std::scoped_lock<std::shared_mutex> r_lock{context->lock};
	return std::bit_cast<const T*>(context->open_files[h].mapped);
}

export template <typename T>
T* map(file_handle h, access_rw_t)
{
	std::scoped_lock<std::shared_mutex> r_lock{context->lock};

	if(!context->open_files[h].rw)
	{
		log::critical("Tried to map readonly file as rw");
		std::unreachable();
	}

	return std::bit_cast<T*>(context->open_files[h].mapped);
}

}

export namespace penumbra
{

void vfs_init()
{
	vfs::context = new vfs::vfs_context_t();

	#if defined __linux__
	struct rlimit lim;
	getrlimit(RLIMIT_NOFILE, &lim);
	lim.rlim_cur = 65536;
	setrlimit(RLIMIT_NOFILE, &lim);
	#endif
}

void vfs_shutdown()
{
	delete vfs::context;
}

}

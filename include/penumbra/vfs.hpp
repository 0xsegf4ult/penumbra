#pragma once

#include <penumbra/types.hpp>
#include <expected>
#include <filesystem>

namespace penumbra
{

void vfs_init();
void vfs_shutdown();

using vfs_path = std::filesystem::path;
using vfs_fd = s32;

enum vfs_access_t
{
	VFS_ACCESS_READ = 0x1,
	VFS_ACCESS_RW	= 0x2,
};

vfs_fd vfs_open(const vfs_path& p, vfs_access_t mode);
void vfs_close(vfs_fd fd);

const u8* vfs_map(vfs_fd fd);
u8* vfs_map_rw(vfs_fd fd);

}

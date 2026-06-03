#ifndef A3F2C1D0_7B4E_4A12_9C83_E5D6F8B21047
#define A3F2C1D0_7B4E_4A12_9C83_E5D6F8B21047
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

#define VFS_O_RDONLY 0x0001
#define VFS_O_WRONLY 0x0002
#define VFS_O_RDWR   0x0003
#define VFS_O_CREAT  0x0004
#define VFS_O_TRUNC  0x0008
#define VFS_O_APPEND 0x0010

#define VFS_SEEK_SET 0
#define VFS_SEEK_CUR 1
#define VFS_SEEK_END 2

struct vfs_node;

struct vfs_file {
    struct vfs_node* node;
    off_t offset;
    unsigned flags;
    unsigned refcount;
};

extern struct vfs_file* vfs_file_open(const char* restrict path, unsigned flags) __nodiscard;
extern struct vfs_file* vfs_file_dupe(struct vfs_file* restrict file) __nodiscard;
extern void vfs_file_close(struct vfs_file* restrict file);
extern ssize_t vfs_file_read(struct vfs_file* restrict file, void* restrict buffer, size_t sz);
extern ssize_t vfs_file_write(struct vfs_file* restrict file, const void* restrict buffer, size_t sz);
extern int vfs_file_seek(struct vfs_file* restrict file, off_t offset, int whence);
extern int vfs_file_ioctl(struct vfs_file* restrict file, unsigned request, void* restrict arg);

#endif /* A3F2C1D0_7B4E_4A12_9C83_E5D6F8B21047 */

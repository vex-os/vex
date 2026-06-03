#ifndef E1D6AD03_9F0E_4E08_869F_24120B327D96
#define E1D6AD03_9F0E_4E08_869F_24120B327D96
#pragma once

#include <core/types.h>

#define VFS_NAME_MAX  255
#define VFS_NAME_SIZE 256

enum {
    VFS_NODE_TYPE_FILE,
    VFS_NODE_TYPE_DIRECTORY,
    VFS_NODE_TYPE_BLK_DEVICE,
    VFS_NODE_TYPE_CHR_DEVICE,
    VFS_NODE_TYPE_SYMLINK,
};

struct vfs_dirent {
    char name[VFS_NAME_SIZE];
    unsigned type;
};

struct vfs_stat {
    ino_t ino;
    dev_t dev;
    off_t size;
    blkcnt_t blocks;
    blksize_t block_size;
    time_t atime;
    time_t mtime;
    time_t ctime;
    unsigned type;
};

#endif /* E1D6AD03_9F0E_4E08_869F_24120B327D96 */

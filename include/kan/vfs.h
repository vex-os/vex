/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_VFS_H__
#define _INCLUDE_KAN_VFS_H__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VFS_FILENAME_LENGTH 256

typedef struct vfs_node_s {
  char name[VFS_FILENAME_LENGTH];
  size_t size;
  size_t nblocks;
  void *direct;
  uint32_t group;
  uint32_t owner;
  bool read_only;
  uint64_t nchildren;
  struct vfs_node_s *parent;
  struct vfs_node_s *children; /* NULL for files */
} vfs_node_t;

typedef struct vfs_super_s {
  uint64_t fs_id;
  char fs_name[VFS_FILENAME_LENGTH];
  size_t fs_block_size;
  uint64_t nblocks;
  uint64_t ninodes;
  void *base;
  vfs_node_t *inodes;
} vfs_super_t;

typedef struct vfs_fsystems_s {
  vfs_super_t *fsystems;
  uint64_t nsystems;
} vfs_fsystems_t;

#define O_CREATE 0x01
#define O_TRUNCATE 0x02
#define O_APPEND 0x04
#define O_READ 0x08
#define O_WRITE 0x10
#define O_RDWR 0x20
#define O_SYNC 0x40

#define S_IRUSR 0x0400
#define S_IWUSR 0x0200
#define S_IXUSR 0x0100
#define S_IRGRP 0x0040
#define S_IWGRP 0x0020
#define S_IXGRP 0x0010
#define S_IROTH 0x0004
#define S_IWOTH 0x0002
#define S_IXOTH 0x0001

vfs_super_t vfs_register_fs(char fs_name[VFS_FILENAME_LENGTH], size_t block_size, void *base);
int vfs_unregister_fs(vfs_super_t fs);
int vfs_mount_fs(const char *rootpath, vfs_super_t fs, bool read_only);
int vfs_unmount_fs(vfs_super_t fs);

void * vfs_create(char * parent_path, const char *pathname);
int vfs_remove(const char *pathname, const char *displace);
void * vfs_open(const char *pathname, short mode);
int vfs_close(int fd);
int vfs_copy(const char *destpath, const char *srcpath, short mode);
size_t vfs_read(int fd, void *buffer, size_t bytes);
size_t vfs_write(int fd, const void *buffer, size_t bytes);

// permissions related stuff
uint64_t vfs_chown(const char *pathname, uint32_t uid, uint32_t gid);
uint64_t vfs_chmod(const char *pathname, short permissions);

#endif /* _INCLUDE_KAN_VFS_H__ */

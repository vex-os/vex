/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_VFS_H__
#define _INCLUDE_KAN_VFS_H__
#include <stddef.h>
#include <stdint.h>

#define VFS_FILENAME_LENGTH 255

typedef struct vfs_node_s {
  char name[VFS_FILENAME_LENGTH];
  size_t size;
  size_t nblocks;
  void *direct;
  uint32_t group;
  uint32_t owner;
  struct vfs_node_s *parent;
  struct vfs_node_s *children; /* NULL for files */
} vfs_node_t;

typedef enum {
  O_CREATE = 0x01,
  O_TRUNCATE = 0x02,
  O_APPEND = 0x04,
  O_READ = 0x08,
  O_WRITE = 0x10,
  O_RDWR = 0x20,
  O_SYNC = 0x40,
} vfs_mode_t;

int vfs_open(const char *restrict pathname, vfs_mode_t mode);

#endif /* _INCLUDE_KAN_VFS_H__ */

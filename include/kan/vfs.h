/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
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
  struct vfs_node_s *parent;
  struct vfs_node_s *children; /* NULL for files */
} vfs_node_t;

#endif /* _INCLUDE_KAN_VFS_H__ */

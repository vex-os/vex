/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_FS_INODE_H__
#define _INCLUDE_FS_INODE_H__
#include <stddef.h>
#include <stdint.h>

#define FS_FILENAME_LENGTH 255

typedef struct fs_inode_t {
  uint8_t name[FS_FILENAME_LENGTH];
  size_t size;
  uint32_t nblocks;
  uintptr_t * direct;
  // parent is used to specify the parent file (i.e. the parent directory of the file)
  struct fs_inode_t parent;
  // children is used to specify contents of a directory, if the file is a directory
  struct fs_inode_t * children;
} fs_inode_t;

#endif /* _INCLUDE_FS_INODE_H__ */

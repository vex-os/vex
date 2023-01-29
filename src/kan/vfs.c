/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/vfs.h>
#include <kan/symbol.h>
#include <kan/kmalloc.h>
#include <string.h>

vfs_super_t vfs_register_fs(char fs_name[VFS_FILENAME_LENGTH], size_t block_size, void *base){
  vfs_super_t return_super;
  strncpy(return_super.fs_name, fs_name, VFS_FILENAME_LENGTH);
  return_super.fs_block_size = block_size;
  return_super.nblocks = 8;
  return_super.ninodes = 1;
  return_super.base = base;
  return_super.inodes = kmalloc(8 * sizeof(vfs_node_t));
  
  return return_super; 
}
EXPORT_SYMBOL(vfs_register_fs);

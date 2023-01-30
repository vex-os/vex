/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/vfs.h>
#include <kan/symbol.h>
#include <kan/kmalloc.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

vfs_super_t vfs_register_fs(char fs_name[VFS_FILENAME_LENGTH], size_t block_size, void *base){
  vfs_super_t return_super;
  strncpy(return_super.fs_name, fs_name, VFS_FILENAME_LENGTH);
  return_super.fs_block_size = block_size;
  return_super.nblocks = 8;
  return_super.ninodes = 1;
  return_super.base = base;
  return_super.inodes = kmalloc(sizeof(vfs_node_t));
  
  return return_super; 
}
EXPORT_SYMBOL(vfs_register_fs);

int vfs_unregister_fs(vfs_super_t fs){
  fs.nblocks = 0;
  fs.ninodes = 0;
  kfree(fs.inodes);
  return 0;
}
EXPORT_SYMBOL(vfs_unregister_fs);

int vfs_mount_fs(const char *rootpath, vfs_super_t fs, bool read_only){
  /* set inode[0] of superblock to root filepath */
  fs.inodes[0].size = fs.fs_block_size;
  fs.inodes[0].nblocks = 1;
  fs.inodes[0].direct = fs.base;
  fs.inodes[0].parent = NULL;
  strncpy(fs.inodes[0].name, rootpath, VFS_FILENAME_LENGTH);

  /* copy root path to memory */
  memcpy(fs.base, &fs.inodes[0], sizeof(vfs_node_t));
  
  /* set read_only mode of superblock to the specified mode */
  for (uint64_t i = 0 ; i < fs.ninodes ; i++){
    fs.inodes[i].read_only = read_only;
  }

  /* copy all inodes (except root inode, that has already been copied) into memory.
     i think this currently disregards directory structure */
  for (uint64_t i = 0 ; i < fs.ninodes ; i++){
    memcpy(fs.base + (i+1 * fs.fs_block_size), &fs.inodes[i], sizeof(vfs_node_t));
  }

  return 0;
}
EXPORT_SYMBOL(vfs_mount_fs);

int vfs_unmount_fs(vfs_super_t fs){
  fs.fs_block_size = 0;
  return 0;
}
EXPORT_SYMBOL(vfs_unmount_fs);

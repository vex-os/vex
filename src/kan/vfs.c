/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/vfs.h>
#include <kan/symbol.h>
#include <kan/kmalloc.h>
#include <kan/kprintf.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Contains a list of all active filesystems*/
vfs_fsystems_t vfs_systems = {
  0,
  0
};

vfs_super_t null_fs = {
  0
};

/* TODO
   The way that vfs_systems currently registers filesystems leaves gaps in the vfs_systems.fsystems pointer.
   Basically, make it so that there aren't gaps in the pointer, by checking for gaps then registering a 
   filesystem in that gap instead of always at the end of the list.
 */
vfs_super_t vfs_register_fs(char fs_name[VFS_FILENAME_LENGTH], size_t block_size, void *base){
  /* I'm not actually sure if this memory need to be free'd later.
     I could probably just implement a limit for how many filesystems can be mounted,
     which would effectively limit the amount of memory vfs_systems.fsystems can take up.
  */
  /* Allocate memory for the upcoming filesystem entry in vfs_systems */
  vfs_systems.fsystems = krealloc(vfs_systems.fsystems, sizeof(vfs_systems.fsystems) + sizeof(vfs_super_t));

  vfs_super_t return_super;
  return_super.fs_id = vfs_systems.nsystems;
  strncpy(return_super.fs_name, fs_name, VFS_FILENAME_LENGTH);
  return_super.fs_block_size = block_size;
  return_super.nblocks = 8;
  return_super.ninodes = 1;
  return_super.base = base;
  return_super.inodes = kmalloc(sizeof(vfs_node_t));
  
  /* Append the filesystem superblock that was just created to the end of the filesystem list */
  memcpy(&vfs_systems.fsystems[return_super.fs_id], &return_super, sizeof(vfs_super_t));
  vfs_systems.nsystems++;
  
  return return_super; 
}
EXPORT_SYMBOL(vfs_register_fs);

int vfs_unregister_fs(vfs_super_t fs){
  memcpy(&vfs_systems.fsystems[fs.fs_id], &null_fs, sizeof(vfs_super_t));
  
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
  fs.inodes[0].direct = fs.base+fs.fs_block_size;
  fs.inodes[0].parent = NULL;
  fs.inodes[0].nchildren = 0;
  fs.inodes[0].children = krealloc(fs.inodes[0].children, fs.inodes[0].nchildren);
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
    memcpy(fs.base + (i+1 * (sizeof(vfs_node_t))), &fs.inodes[i], sizeof(vfs_node_t));
  }

  return 0;
}
EXPORT_SYMBOL(vfs_mount_fs);

int vfs_unmount_fs(vfs_super_t fs){
  fs.fs_name[0] = 0;
  return 0;
}
EXPORT_SYMBOL(vfs_unmount_fs);

int vfs_create(char *parent_path, const char *pathname){
  vfs_node_t new_file = {
    0
  };

  /* look for existence of <parent_path> */ 
  for (uint64_t i = 0 ; i < vfs_systems.nsystems ; i++){
    if (memcmp(&vfs_systems.fsystems[i], &null_fs, sizeof(vfs_super_t)) != 0){
      for (uint64_t j = 0 ; j < vfs_systems.fsystems[i].ninodes ; j++){	
	if (strncmp(vfs_systems.fsystems[i].inodes[j].name, parent_path, VFS_FILENAME_LENGTH) == 0){
	  /* <parent_path> was found, and now new_file is being created within <parent_path>  */
	  strcpy(new_file.name, parent_path);
	  strcat(new_file.name, pathname);

	  new_file.size = vfs_systems.fsystems[i].fs_block_size * 8;
	  new_file.nblocks = 8;
	  new_file.direct = (void *) vfs_systems.fsystems[i].base + (vfs_systems.fsystems[i].nblocks * vfs_systems.fsystems[i].fs_block_size);
	  vfs_systems.fsystems[i].nblocks += 8;

	  new_file.read_only = false;	  
	  new_file.nchildren = 0;
	  
	  memcpy(&vfs_systems.fsystems[i].inodes[j+1], &new_file, sizeof(new_file));
	  strcpy(vfs_systems.fsystems[i].inodes[vfs_systems.fsystems[i].ninodes+1].name, parent_path);
	  vfs_systems.fsystems[i].ninodes++;

	  /* TODO
	     Handle children and parents within inodes
	   */
	  
	  pr_inform("vfs: %s location=%p",new_file.name, new_file.direct);
	  return 0;
	}
      }
    }
  }
  pr_inform("vfs: directory not found");
  return -1;
}
EXPORT_SYMBOL(vfs_create);

/* TODO
   Implement displacement
*/
int vfs_remove(const char *pathname, const char *displace){
  vfs_node_t null_file = {
    0
  };
  
  for (uint64_t i = 0 ; i < vfs_systems.nsystems ; i++){
    if (memcmp(&vfs_systems.fsystems[i], &null_fs, sizeof(vfs_super_t)) != 0){
      for (uint64_t j = 0 ; j < vfs_systems.fsystems[i].ninodes ; j++){
	if (strncmp(vfs_systems.fsystems[i].inodes[j].name, pathname, VFS_FILENAME_LENGTH) == 0){

	  vfs_systems.fsystems[i].nblocks -= vfs_systems.fsystems[i].inodes[j].nblocks;
	  memcpy(&vfs_systems.fsystems[i].inodes[j], &null_file, sizeof(vfs_node_t));

	  return 0;
	}
      }
    }
  }
  pr_inform("vfs: directory not found");
  return -1;
}
EXPORT_SYMBOL(vfs_remove);

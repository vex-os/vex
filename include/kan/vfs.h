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
  O_SYNC = 0x40
} vfs_mode_t;

typedef enum {
  S_IRUSR = 0x0400,
  S_IWUSR = 0x0200,
  S_IXUSR = 0x0100,
  S_IRGRP = 0x0040,
  S_IWGRP = 0x0020,
  S_IXGRP = 0x0010,
  S_IROTH = 0x0004,
  S_IWOTH = 0x0002,
  S_IXOTH = 0x0001
} vfs_perm_t;

int vfs_create(const char *pathname); /* returns whether or not creation of file was successful */

/* vfs_remove
   if <displace> is not NULL and is a directory and <pathname> is a directory, move the contents of <pathname>
   to directory <displace>
 */
int vfs_remove(const char *pathname, const char *displace); /* returns whether or not deletion was successful */
int vfs_open(const char *pathname, vfs_mode_t mode); /* returns file descriptor */
int vfs_close(int fd); /* returns whether or not the closing of <fd> was successful */

/* vfs_copy
   returns whether or not copy was successful.
   only modes that may be used are O_TRUNCATE, O_CREATE, O_APPEND and O_SYNC.
   any other modes causes vfs_copy to return an error.
   - if used in truncate mode and file at <destpath> exists, contents of file
     are deleted and overwritten with contents of <srcpath>.
   - if used in create mode and file at <destpath> doesn't exist, create file then
     write contents of <srcpath> to it.
   - if used in append mode and file at <destpath> exists, append contents of <srcpath>
     to contents of <destpath>
   - if used in sync mode and file at <destpath> exists, do the same thing as when used
     in truncate mode, but instead wait for any io operatings to <destpath> to complete
     before copying contents of <srcpath>
*/
int vfs_copy(const char *destpath, const char *srcpath, vfs_mode_t mode);
size_t vfs_read(int fd, void *buffer, size_t bytes); /* returns number of bytes read */
size_t vfs_write(int fd, const void *buffer, size_t bytes); /* returns number of bytes written */

// permissions related stuff
uint64_t vfs_chown(const char *pathname, uint32_t uid, uint32_t gid); /* returns previous gid*32 |= previous uid */
uint64_t vfs_chmod(const char *pathname, vfs_perm_t mode); /* returns previous permissions */

#endif /* _INCLUDE_KAN_VFS_H__ */

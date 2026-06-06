#ifndef C79FAE96_D7EE_42CD_836F_A55F8731CA80
#define C79FAE96_D7EE_42CD_836F_A55F8731CA80
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/vfs_vnode.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct file {
    struct vnode* node;
    unsigned refcount;
    uintptr_t offset;
    unsigned flags;
};

struct file* file_open(const char* restrict path, unsigned flags) __nodiscard;
struct file* file_dupe(struct file* restrict fp) __nodiscard;
void file_close(struct file* restrict fp);

ssize_t file_read(struct file* restrict fp, void* restrict buffer, size_t sz);
ssize_t file_write(struct file* restrict fp, const void* restrict buffer, size_t sz);

int file_seek(struct file* restrict fp, ptrdiff_t offset, int origin);
int file_ioctl(struct file* restrict fp, uint32_t request, void* restrict argument);

#endif /* C79FAE96_D7EE_42CD_836F_A55F8731CA80 */

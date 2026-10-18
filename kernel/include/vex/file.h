#pragma once

#include <vex/compiler.h>
#include <vex/fcntl.h>
#include <vex/types.h>
#include <vex/vnode.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct file {
    struct vnode* node;
    unsigned refcount;
    unsigned flags;
    off_t offset;
};

int file_open(struct vnode* restrict stem, const char* restrict path, unsigned flags, mode_t mode, const struct cred* restrict cred,
    struct file** restrict out) __nodiscard;
struct file* file_dupe(struct file* restrict fp) __nodiscard;
void file_close(struct file* restrict fp);

ssize_t file_read(struct file* restrict fp, void* restrict buffer, size_t sz);
ssize_t file_write(struct file* restrict fp, const void* restrict buffer, size_t sz);

int file_seek(struct file* restrict fp, off_t offset, int origin);
int file_ioctl(struct file* restrict fp, uint32_t request, uintptr_t argument);

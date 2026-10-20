#pragma once

#include <vex/compiler.h>
#include <vex/fcntl.h>
#include <vex/types.h>
#include <vex/vnode.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct file_s file_t;

struct file_s {
    vnode_t* node;
    unsigned refcount;
    unsigned flags;
    off_t offset;
};

int file_open(vnode_t* restrict base_dir, const char* restrict path, unsigned flags, mode_t mode, const credentials_t* restrict credentials,
    file_t** restrict out) __nodiscard;
int file_open_node(vnode_t* restrict node, unsigned flags, const credentials_t* restrict credentials, file_t** restrict out) __nodiscard;
file_t* file_dupe(file_t* restrict file) __nodiscard;
void file_close(file_t* restrict file);

ssize_t file_read(file_t* restrict file, void* restrict buffer, size_t size);
ssize_t file_write(file_t* restrict file, const credentials_t* restrict credentials, const void* restrict buffer, size_t size);

int file_truncate(file_t* restrict file, const credentials_t* restrict credentials, off_t size);
int file_seek(file_t* restrict file, off_t offset, int origin);
int file_ioctl(file_t* restrict file, uint32_t request, uintptr_t argument);

#ifndef DCC199D9_02EB_45AE_AB94_7E18F82E3014
#define DCC199D9_02EB_45AE_AB94_7E18F82E3014
#pragma once

#include <core/cdefs.h>
#include <stddef.h>
#include <stdint.h>

struct blkdev;
struct blkdev_ops {
    int (*read)(struct blkdev* restrict device, uintptr_t lba, size_t count, void* restrict buffer);
    int (*write)(struct blkdev* restrict device, uintptr_t lba, size_t count, const void* restrict buffer);
    int (*flush)(struct blkdev* restrict device);
    void (*destroy)(struct blkdev* restrict device);
};

struct blkdev_private {
    struct blkdev* next;
};

struct blkdev {
    struct blkdev_private priv;
    const struct blkdev_ops* ops;
    char identifier[64];
    size_t sector_cnt;
    size_t sector_sz;
    void* ctxdata;
};

extern int register_blkdev(struct blkdev* restrict device);
extern int unregister_blkdev(struct blkdev* restrict device);
extern struct blkdev* blkdev_find(const char* name) __nodiscard;

static __always_inline inline int blkdev_read(struct blkdev* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    return device->ops->read(device, lba, count, buffer);
}

static __always_inline inline int blkdev_write(struct blkdev* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    return device->ops->write(device, lba, count, buffer);
}

static __always_inline inline int blkdev_flush(struct blkdev* restrict device)
{
    if(device->ops->flush)
        return device->ops->flush(device);
    return 0;
}

#endif /* DCC199D9_02EB_45AE_AB94_7E18F82E3014 */

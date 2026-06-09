#ifndef DCC199D9_02EB_45AE_AB94_7E18F82E3014
#define DCC199D9_02EB_45AE_AB94_7E18F82E3014
#pragma once

#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/types.h>

struct blk_name;

struct blk_device;
struct blk_device_ops {
    int (*read)(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer);
    int (*write)(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer);
    int (*flush)(const struct blk_device* restrict device);
    void (*release)(const struct blk_device* restrict device);
};

struct blk_device_private {
    struct blk_device* next;
    struct blk_name* name;
    unsigned name_iterator;
};

#define BLK_FLAG_PRESENT   0x0001 // Media is present and ready
#define BLK_FLAG_PARTITION 0x0002 // Device is a partition device
#define BLK_FLAG_HIDDEN    0x0004 // Device is not added to devfs

struct blk_device {
    struct blk_device_private priv;
    const struct blk_device_ops* ops;
    char identifier[64];
    size_t sector_count;
    size_t sector_size;
    unsigned partition;
    unsigned flags;
    void* ctxdata;
};

extern struct blk_device* blk_devices_head;

int add_blk_device_root(struct blk_device* restrict device, struct blk_name* restrict name, unsigned iterator, unsigned index);
int add_blk_device_part(struct blk_device* restrict device, const struct blk_device* restrict root, unsigned partition);
int add_blk_device_named(struct blk_device* restrict device);
int remove_blk_device(struct blk_device* restrict device);

const struct blk_device* find_blk_device(const char* restrict name) __nodiscard;

static __always_inline int blk_read(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer)
{
    if(0 == (device->flags & BLK_FLAG_PRESENT))
        return -ENODEV;
    return device->ops->read(device, lba, count, buffer);
}

static __always_inline int blk_write(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer)
{
    if(0 == (device->flags & BLK_FLAG_PRESENT))
        return -ENODEV;
    return device->ops->write(device, lba, count, buffer);
}

static __always_inline int blk_flush(const struct blk_device* restrict device)
{
    if(0 == (device->flags & BLK_FLAG_PRESENT))
        return -ENODEV;
    if(device->ops->flush)
        return device->ops->flush(device);
    return 0;
}

#endif /* DCC199D9_02EB_45AE_AB94_7E18F82E3014 */

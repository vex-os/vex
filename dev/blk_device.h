#ifndef DCC199D9_02EB_45AE_AB94_7E18F82E3014
#define DCC199D9_02EB_45AE_AB94_7E18F82E3014
#pragma once

#include <core/cdefs.h>
#include <core/errno.h>
#include <stddef.h>
#include <stdint.h>

struct blk_name;

struct blk_device;
struct blk_device_ops {
    int (*read)(const struct blk_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer);
    int (*write)(const struct blk_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer);
    int (*flush)(const struct blk_device* restrict device);
    void (*destroy)(const struct blk_device* restrict device);
};

struct blk_device_private {
    struct blk_device* next;
    struct blk_name* name;
    unsigned name_iterator;
};

#define BLK_FLAG_PRESENT 0x0001 ///< Media is present and ready

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

extern int blk_device_register_root(struct blk_device* restrict device, struct blk_name* restrict name, unsigned index);
extern int blk_device_register_part(struct blk_device* restrict device, struct blk_name* restrict name, unsigned index, unsigned partition);
extern int blk_device_register_custom(struct blk_device* restrict device);
extern int blk_device_unregister(struct blk_device* restrict device);
extern const struct blk_device* blk_device_find(const char* name) __nodiscard;

static __always_inline inline int blk_device_read(const struct blk_device* restrict device, uintptr_t lba, size_t count,
    void* restrict buffer)
{
    if(0 == (device->flags & BLK_FLAG_PRESENT))
        return ENODEV;
    return device->ops->read(device, lba, count, buffer);
}

static __always_inline inline int blk_device_write(const struct blk_device* restrict device, uintptr_t lba, size_t count,
    const void* restrict buffer)
{
    if(0 == (device->flags & BLK_FLAG_PRESENT))
        return ENODEV;
    return device->ops->write(device, lba, count, buffer);
}

static __always_inline inline int blk_device_flush(const struct blk_device* restrict device)
{
    if(0 == (device->flags & BLK_FLAG_PRESENT))
        return ENODEV;
    if(device->ops->flush)
        return device->ops->flush(device);
    return 0;
}

#endif /* DCC199D9_02EB_45AE_AB94_7E18F82E3014 */

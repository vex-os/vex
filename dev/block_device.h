#ifndef DCC199D9_02EB_45AE_AB94_7E18F82E3014
#define DCC199D9_02EB_45AE_AB94_7E18F82E3014
#pragma once

#include <core/cdefs.h>
#include <stddef.h>
#include <stdint.h>

struct block_device;
struct block_device_ops {
    int (*read)(struct block_device* restrict device, uintptr_t lba, size_t count, void* restrict buffer);
    int (*write)(struct block_device* restrict device, uintptr_t lba, size_t count, const void* restrict buffer);
    int (*flush)(struct block_device* restrict device);
    void (*destroy)(struct block_device* restrict device);
};

struct block_device_private {
    struct block_device* next;
};

struct block_device {
    struct block_device_private priv;
    const struct block_device_ops* ops;
    char identifier[64];
    size_t sector_count;
    size_t sector_size;
    void* ctxdata;
};

extern int block_device_register(struct block_device* restrict device);
extern int block_device_unregister(struct block_device* restrict device);
extern struct block_device* block_device_find(const char* name) __nodiscard;

static __always_inline inline int block_device_read(struct block_device* restrict device, uintptr_t lba, size_t count,
    void* restrict buffer)
{
    return device->ops->read(device, lba, count, buffer);
}

static __always_inline inline int block_device_write(struct block_device* restrict device, uintptr_t lba, size_t count,
    const void* restrict buffer)
{
    return device->ops->write(device, lba, count, buffer);
}

static __always_inline inline int block_device_flush(struct block_device* restrict device)
{
    if(device->ops->flush)
        return device->ops->flush(device);
    return 0;
}

#endif /* DCC199D9_02EB_45AE_AB94_7E18F82E3014 */

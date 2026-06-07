#ifndef B2600BF7_96F9_4B83_8FA8_6C2C28124FF2
#define B2600BF7_96F9_4B83_8FA8_6C2C28124FF2
#pragma once

#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/types.h>

struct chr_name;

struct chr_device;
struct chr_device_ops {
    ssize_t (*read)(const struct chr_device* restrict device, off_t offset, void* restrict buffer, size_t sz);
    ssize_t (*write)(const struct chr_device* restrict device, off_t offset, const void* restrict buffer, size_t sz);
    int (*ioctl)(const struct chr_device* restrict device, uint32_t request, void* restrict arg);
    void (*release)(const struct chr_device* restrict device);
};

struct chr_device_private {
    struct chr_device* next;
    struct chr_name* name;
    unsigned name_iterator;
};

struct chr_device {
    struct chr_device_private priv;
    const struct chr_device_ops* ops;
    char identifier[64];
    void* ctxdata;
};

extern struct chr_device* chr_devices_head;

int add_chr_device(struct chr_device* restrict device, struct chr_name* restrict name);
int add_chr_device_named(struct chr_device* restrict device, const char* restrict name);
int remove_chr_device(struct chr_device* restrict device);

const struct chr_device* find_chr_device(const char* restrict name) __nodiscard;

static __always_inline ssize_t chr_device_read(const struct chr_device* restrict device, off_t offset, void* restrict buffer, size_t sz)
{
    if(device->ops->read == nullptr)
        return -ENOSYS;
    return device->ops->read(device, offset, buffer, sz);
}

static __always_inline ssize_t chr_device_write(const struct chr_device* restrict device, off_t offset, const void* restrict buffer,
    size_t sz)
{
    if(device->ops->write == nullptr)
        return -ENOSYS;
    return device->ops->write(device, offset, buffer, sz);
}

static __always_inline int chr_device_ioctl(const struct chr_device* restrict device, uint32_t request, void* restrict arg)
{
    if(device->ops->ioctl == nullptr)
        return -ENOTTY;
    return device->ops->ioctl(device, request, arg);
}

#endif /* B2600BF7_96F9_4B83_8FA8_6C2C28124FF2 */

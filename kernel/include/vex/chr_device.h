#pragma once

#include <vex/atomic.h>
#include <vex/compiler.h>
#include <vex/errno.h>
#include <vex/mutex.h>
#include <vex/types.h>

typedef struct chr_name_s chr_name_t;
typedef struct console_s console_t;

typedef struct chr_device_s chr_device_t;
typedef struct chr_device_ops_s chr_device_ops_t;
typedef struct chr_device_private_s chr_device_private_t;

struct chr_device_ops_s {
    /// @pre This is the first open, and the device's console is released
    /// @note Set the hardware up here, not at probe, as the console owns it until now;
    /// a failure leaves the device to nobody
    int (*open)(const chr_device_t* restrict device, unsigned flags);

    /// @pre This is the last close, and the device wasn't removed while open
    void (*close)(const chr_device_t* restrict device);

    ssize_t (*read)(const chr_device_t* restrict device, off_t offset, void* restrict buffer, size_t size);
    ssize_t (*write)(const chr_device_t* restrict device, off_t offset, const void* restrict buffer, size_t size);
    int (*ioctl)(const chr_device_t* restrict device, uint32_t request, uintptr_t argument);

    /// Checks a mapping of the device's memory before it is made
    /// @pre offset and length are page aligned
    /// @retval -ENXIO The range runs past the end of the device's memory
    /// @note nullptr, as is resolve_page, for a device without memory
    int (*mmap)(const chr_device_t* restrict device, off_t offset, size_t length);

    /// Gives the physical address and memory type (a VPROT_CACHE_* value) of the page at offset in the device's memory
    /// @pre offset is page aligned; the caller holds a spinlock, so this can't sleep
    /// @retval -ENXIO offset is past the end of the device's memory
    int (*resolve_page)(const chr_device_t* restrict device, off_t offset, uintptr_t* restrict phys, unsigned* restrict cache);

    void (*release)(const chr_device_t* restrict device);
};

struct chr_device_private_s {
    chr_device_t* next;
    chr_name_t* name;
    unsigned name_iterator;
    unsigned refcount;
    bool removed;
    mutex_t open_lock; // Serializes open_count with the driver's open and close
    unsigned open_count;
    bool exclusive; // Opened with O_EXCL; other opens fail with -EBUSY until the last close
};

struct chr_device_s {
    chr_device_private_t priv;
    const chr_device_ops_t* ops;
    char name[64];
    console_t* console; // Writes to the device until its first open; may be nullptr
    void* ctxdata;
};

int chr_device_add(chr_device_t* restrict device, chr_name_t* restrict name);
int chr_device_add_named(chr_device_t* restrict device, const char* restrict name);

/// Drops the registry's reference; holders of others keep the structure
/// @pre The driver fails its own calls with -ENODEV and has woken those that sleep, as a call
/// that passed the chr layer's check may still reach it
/// @post Calls through the chr layer fail with -ENODEV, a mapping of the device's memory takes SIGBUS
/// on its next access, and the driver gets nothing but release
int chr_device_remove(chr_device_t* restrict device);

void chr_device_incref(const chr_device_t* restrict device);
void chr_device_decref(const chr_device_t* restrict device);

int chr_device_open(const chr_device_t* restrict device, unsigned flags);
void chr_device_close(const chr_device_t* restrict device);

const chr_device_t* chr_device_find(const char* restrict name) __nodiscard;

/// @pre The caller holds chr_event_lock, which keeps the device list still
void chr_device_for_each(void (*callback)(const chr_device_t* restrict device, void* restrict arg), void* restrict arg);

static __always_inline ssize_t chr_device_read(const chr_device_t* restrict device, off_t offset, void* restrict buffer, size_t size)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->read == nullptr)
        return -ENOSYS;
    return device->ops->read(device, offset, buffer, size);
}

static __always_inline ssize_t chr_device_write(const chr_device_t* restrict device, off_t offset, const void* restrict buffer, size_t size)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->write == nullptr)
        return -ENOSYS;
    return device->ops->write(device, offset, buffer, size);
}

static __always_inline int chr_device_ioctl(const chr_device_t* restrict device, uint32_t request, uintptr_t argument)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->ioctl == nullptr)
        return -ENOTTY;
    return device->ops->ioctl(device, request, argument);
}

static __always_inline int chr_device_mmap(const chr_device_t* restrict device, off_t offset, size_t length)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->mmap == nullptr)
        return -ENODEV;
    return device->ops->mmap(device, offset, length);
}

static __always_inline int chr_device_resolve_page(const chr_device_t* restrict device, off_t offset, uintptr_t* restrict phys,
    unsigned* restrict cache)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->resolve_page == nullptr)
        return -ENODEV;
    return device->ops->resolve_page(device, offset, phys, cache);
}

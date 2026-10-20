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
    /// Called on the first open, after the device's console is released: the console owns the hardware
    /// until then, so the driver sets it up here and not at probe. A failure leaves the device to nobody
    int (*open)(const chr_device_t* restrict device, unsigned flags);

    /// Called on the last close, unless the device was removed while open
    void (*close)(const chr_device_t* restrict device);

    ssize_t (*read)(const chr_device_t* restrict device, off_t offset, void* restrict buffer, size_t size);
    ssize_t (*write)(const chr_device_t* restrict device, off_t offset, const void* restrict buffer, size_t size);
    int (*ioctl)(const chr_device_t* restrict device, uint32_t request, uintptr_t argument);

    /// Checks a mapping of page_count pages of the device's memory from first_page before it is made.
    /// Both mmap and resolve_page are nullptr for a device without memory
    /// @retval -ENXIO The range runs past the end of the device's memory
    int (*mmap)(const chr_device_t* restrict device, uint64_t first_page, uint64_t page_count);

    /// Gives the physical address and memory type (a VPROT_CACHE_* value) of page index of the device's memory
    /// @retval -ENXIO The page is past the end of the device's memory
    int (*resolve_page)(const chr_device_t* restrict device, uint64_t index, uintptr_t* restrict phys, unsigned* restrict cache);

    /// Called once the last reference is gone
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

/// Takes the device out of the registry and drops the reference it held there; holders of
/// other references keep the structure, but their calls fail with -ENODEV from now on
/// @pre The driver already fails its own calls with -ENODEV and has woken those that sleep;
/// a call that passed the check here may still reach it
/// @post The chr layer calls nothing in the driver but release
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

static __always_inline int chr_device_mmap(const chr_device_t* restrict device, uint64_t first_page, uint64_t page_count)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->mmap == nullptr)
        return -ENODEV;
    return device->ops->mmap(device, first_page, page_count);
}

static __always_inline int chr_device_resolve_page(const chr_device_t* restrict device, uint64_t index, uintptr_t* restrict phys,
    unsigned* restrict cache)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->resolve_page == nullptr)
        return -ENODEV;
    return device->ops->resolve_page(device, index, phys, cache);
}

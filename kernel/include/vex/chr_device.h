#pragma once

#include <vex/atomic.h>
#include <vex/compiler.h>
#include <vex/dev_name.h>
#include <vex/errno.h>
#include <vex/mutex.h>
#include <vex/types.h>

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

    /// Sets up the state of one open file, which the
    /// other calls on that file get as file_ctxdata
    /// @pre On the first open, open has run before it
    int (*open_file)(const chr_device_t* restrict device, unsigned flags, void** restrict file_ctxdata);

    /// @note Runs even on a device removed while open, as the file's
    /// state is freed here; on the last close, close runs after it
    void (*close_file)(const chr_device_t* restrict device, void* restrict file_ctxdata);

    ssize_t (*read)(const chr_device_t* restrict device, void* restrict file_ctxdata, off_t offset, void* restrict buffer, size_t size,
        unsigned flags);
    ssize_t (*write)(const chr_device_t* restrict device, void* restrict file_ctxdata, off_t offset, const void* restrict buffer,
        size_t size, unsigned flags);

    /// Moves the file's offset by the device's own rules, for devices whose offset isn't a byte position
    /// @param position The file's offset now
    /// @return The new offset
    /// @note nullptr leaves the offset to the VFS
    off_t (*seek)(const chr_device_t* restrict device, void* restrict file_ctxdata, off_t position, off_t offset, int origin);

    int (*ioctl)(const chr_device_t* restrict device, void* restrict file_ctxdata, uint32_t request, uintptr_t argument);

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
    dev_name_t name;
    dev_t dev;
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

/// @param index The n of the name, such as the card's port; DEV_NAME_NONE for a device that is its unit's only one
int chr_device_add(chr_device_t* restrict device, dev_unit_t* restrict unit, unsigned index);

/// Names the device with the prefix alone, for a device the system has only one of
/// @pre The caller holds the prefix through dev_prefix_claim, until the device is removed
int chr_device_add_unitless(chr_device_t* restrict device, dev_prefix_t* restrict prefix);

/// Drops the registry's reference; holders of others keep the structure
/// @pre The driver fails its own calls with -ENODEV and has woken those that sleep, as a call
/// that passed the chr layer's check may still reach it
/// @post Calls through the chr layer fail with -ENODEV, a mapping of the device's memory takes SIGBUS
/// on its next access, and the driver gets nothing but release
int chr_device_remove(chr_device_t* restrict device);

/// Unmaps the device's memory from every address space, so the next access faults on it again
/// @pre Interrupts are enabled, as the event subscribers may sleep
/// @note For a driver whose memory changed layout, such as a framebuffer after a mode change
void chr_device_revoke_mappings(const chr_device_t* restrict device);

void chr_device_incref(const chr_device_t* restrict device);
void chr_device_decref(const chr_device_t* restrict device);

/// @param file_ctxdata Gets what the driver's open_file made, if it has one
int chr_device_open(const chr_device_t* restrict device, unsigned flags, void** restrict file_ctxdata);

/// Makes further opens fail with -EBUSY until the last close, as an O_EXCL open does
void chr_device_set_exclusive(const chr_device_t* restrict device, bool exclusive);

/// @return Whether an O_EXCL open or TIOCEXCL keeps others from opening the device
bool chr_device_is_exclusive(const chr_device_t* restrict device) __nodiscard;

/// Makes further opens fail with -EBUSY, unless the device is open
/// @retval -EBUSY The device is open
/// @note For a device about to be removed, so that no open reaches its driver meanwhile
int chr_device_refuse_opens(const chr_device_t* restrict device);
void chr_device_close(const chr_device_t* restrict device, void* restrict file_ctxdata);

const chr_device_t* chr_device_find(const char* restrict name) __nodiscard;

/// @pre The caller holds chr_event_lock, which keeps the device list still
void chr_device_for_each(void (*callback)(const chr_device_t* restrict device, void* restrict arg), void* restrict arg);

static __always_inline ssize_t chr_device_read(const chr_device_t* restrict device, void* restrict file_ctxdata, off_t offset,
    void* restrict buffer, size_t size, unsigned flags)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->read == nullptr)
        return -ENOSYS;
    return device->ops->read(device, file_ctxdata, offset, buffer, size, flags);
}

static __always_inline ssize_t chr_device_write(const chr_device_t* restrict device, void* restrict file_ctxdata, off_t offset,
    const void* restrict buffer, size_t size, unsigned flags)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->write == nullptr)
        return -ENOSYS;
    return device->ops->write(device, file_ctxdata, offset, buffer, size, flags);
}

/// @retval -ENOSYS The device leaves the offset to the VFS
static __always_inline off_t chr_device_seek(const chr_device_t* restrict device, void* restrict file_ctxdata, off_t position, off_t offset,
    int origin)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->seek == nullptr)
        return -ENOSYS;
    return device->ops->seek(device, file_ctxdata, position, offset, origin);
}

static __always_inline int chr_device_ioctl(const chr_device_t* restrict device, void* restrict file_ctxdata, uint32_t request,
    uintptr_t argument)
{
    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE))
        return -ENODEV;
    if(device->ops->ioctl == nullptr)
        return -ENOTTY;
    return device->ops->ioctl(device, file_ctxdata, request, argument);
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

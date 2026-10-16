#include <vex/atomic.h>
#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/blk_name.h>
#include <vex/kmalloc.h>
#include <vex/limits.h>
#include <vex/spinlock.h>
#include <vex/sprintf.h>
#include <vex/string.h>

// Event handlers may do I/O so they run outside the lock
static spinlock_type blk_devices_lock = SPINLOCK_INIT;
static struct blk_device* blk_devices_head = nullptr;

static int blk_device_register_internal(struct blk_device* restrict device)
{
    spinlock_acquire(&blk_devices_lock);

    for(struct blk_device* it = blk_devices_head; it; it = it->priv.next) {
        if(it == device) {
            spinlock_release(&blk_devices_lock);
            return -EEXIST;
        }
    }

    atomic_store(&device->priv.refcount, 1, ATOMIC_RELAXED);

    device->priv.next = blk_devices_head;
    blk_devices_head = device;

    spinlock_release(&blk_devices_lock);

    blk_event_notify(BLK_EVENT_DEVICE_ADDED, device);

    return 0;
}

int add_blk_device_root(struct blk_device* restrict device, struct blk_name* restrict name, unsigned iterator, unsigned index)
{
    device->priv.name = name;
    device->priv.name_iterator = iterator;
    device->partition = UINT_MAX;

    snprintf(device->identifier, sizeof(device->identifier), "%s%un%u", name->device_prefix, device->priv.name_iterator, index + 1);

    return blk_device_register_internal(device);
}

int add_blk_device_part(struct blk_device* restrict device, const struct blk_device* restrict root, unsigned partition)
{
    device->priv.name = root->priv.name;
    device->priv.name_iterator = root->priv.name_iterator;
    device->partition = partition;

    snprintf(device->identifier, sizeof(device->identifier), "%sp%u", root->identifier, partition + 1);

    return blk_device_register_internal(device);
}

int add_blk_device_named(struct blk_device* restrict device)
{
    device->priv.name = nullptr;
    device->priv.name_iterator = 0;
    device->partition = UINT_MAX;
    return blk_device_register_internal(device);
}

int remove_blk_device(struct blk_device* restrict device)
{
    spinlock_acquire(&blk_devices_lock);

    struct blk_device** prev = &blk_devices_head;

    while(*prev && *prev != device) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        spinlock_release(&blk_devices_lock);
        return -ENODEV;
    }

    *prev = device->priv.next;
    device->priv.next = nullptr;

    spinlock_release(&blk_devices_lock);

    // Holders of a reference keep the structure, not
    // the hardware; their I/O fails from here on
    bool was_present = device->flags & BLK_FLAG_PRESENT;
    device->flags &= ~BLK_FLAG_PRESENT;

    if(was_present) {
        blk_event_notify(BLK_EVENT_MEDIA_DISCONNECTED, device);
    }

    blk_event_notify(BLK_EVENT_DEVICE_REMOVED, device);

    blk_device_decref(device);

    return 0;
}

void blk_device_incref(const struct blk_device* restrict device)
{
    struct blk_device* mutable_device = (struct blk_device*)(device);
    atomic_fetch_add(&mutable_device->priv.refcount, 1, ATOMIC_RELAXED);
}

void blk_device_decref(const struct blk_device* restrict device)
{
    struct blk_device* mutable_device = (struct blk_device*)(device);

    if(1 == atomic_fetch_sub(&mutable_device->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        if(device->ops->release) {
            device->ops->release(device);
        }
    }
}

const struct blk_device* find_blk_device(const char* restrict name)
{
    spinlock_acquire(&blk_devices_lock);

    for(struct blk_device* it = blk_devices_head; it; it = it->priv.next) {
        if(0 == strcmp(it->identifier, name)) {
            blk_device_incref(it);
            spinlock_release(&blk_devices_lock);
            return it;
        }
    }

    spinlock_release(&blk_devices_lock);

    return nullptr;
}

void iterate_blk_devices(void (*callback)(const struct blk_device* restrict device, void* restrict arg), void* restrict arg)
{
    spinlock_acquire(&blk_devices_lock);

    for(struct blk_device* it = blk_devices_head; it; it = it->priv.next) {
        callback(it, arg);
    }

    spinlock_release(&blk_devices_lock);
}

int blk_read_sector_prefix(const struct blk_device* restrict device, uintptr_t lba, void* restrict buffer, size_t size)
{
    if(device->sector_size < size) {
        return -EINVAL;
    }

    void* sector = kmalloc(device->sector_size);

    if(sector == nullptr) {
        return -ENOMEM;
    }

    int retval = blk_read(device, lba, 1, sector);

    if(retval == 0) {
        memcpy(buffer, sector, size);
    }

    kfree(sector);

    return retval;
}

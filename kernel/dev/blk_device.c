#include <vex/atomic.h>
#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/dev_name.h>
#include <vex/kmalloc.h>
#include <vex/spinlock.h>
#include <vex/string.h>

// Writers also hold blk_event_lock, which keeps the list still for
// blk_device_for_each; the spinlock is for lookups from anywhere else
static spinlock_t blk_devices_lock = SPINLOCK_INIT;
static blk_device_t* blk_devices_head = nullptr;

static int blk_device_register_internal(blk_device_t* restrict device)
{
    blk_event_lock();
    spinlock_acquire(&blk_devices_lock);

    for(blk_device_t* it = blk_devices_head; it; it = it->priv.next) {
        if(it == device) {
            spinlock_release(&blk_devices_lock);
            blk_event_unlock();
            return -EEXIST;
        }
    }

    atomic_store(&device->priv.refcount, 1, ATOMIC_RELAXED);

    device->priv.next = blk_devices_head;
    blk_devices_head = device;

    spinlock_release(&blk_devices_lock);

    blk_event_notify(BLK_EVENT_DEVICE_ADDED, device);

    blk_event_unlock();

    return 0;
}

static int name_device(blk_device_t* restrict device, const dev_name_t* restrict name)
{
    int retval = dev_name_format(name, device->name, sizeof(device->name));

    if(retval < 0) {
        return retval;
    }

    device->priv.name = *name;
    device->priv.dev = dev_name_to_dev(name);

    return 0;
}

int blk_device_add_root(blk_device_t* restrict device, dev_unit_t* restrict unit, unsigned index)
{
    dev_name_t name = {};
    name.unit = unit;
    name.index = index;
    name.partition = DEV_NAME_NONE;

    int retval = name_device(device, &name);

    if(retval < 0) {
        return retval;
    }

    return blk_device_register_internal(device);
}

int blk_device_add_part(blk_device_t* restrict device, const blk_device_t* restrict root, unsigned partition)
{
    dev_name_t name = root->priv.name;
    name.partition = partition;

    int retval = name_device(device, &name);

    if(retval < 0) {
        return retval;
    }

    return blk_device_register_internal(device);
}

int blk_device_remove(blk_device_t* restrict device)
{
    blk_event_lock();
    spinlock_acquire(&blk_devices_lock);

    blk_device_t** prev = &blk_devices_head;

    while(*prev && *prev != device) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        spinlock_release(&blk_devices_lock);
        blk_event_unlock();
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

    blk_event_unlock();

    blk_device_decref(device);

    return 0;
}

void blk_device_incref(const blk_device_t* restrict device)
{
    blk_device_t* mutable_device = (blk_device_t*)(device);
    atomic_fetch_add(&mutable_device->priv.refcount, 1, ATOMIC_RELAXED);
}

void blk_device_decref(const blk_device_t* restrict device)
{
    blk_device_t* mutable_device = (blk_device_t*)(device);

    if(1 == atomic_fetch_sub(&mutable_device->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        if(device->ops->release) {
            device->ops->release(device);
        }
    }
}

const blk_device_t* blk_device_find(const char* restrict name)
{
    spinlock_acquire(&blk_devices_lock);

    for(blk_device_t* it = blk_devices_head; it; it = it->priv.next) {
        if(0 == strcmp(it->name, name)) {
            blk_device_incref(it);
            spinlock_release(&blk_devices_lock);
            return it;
        }
    }

    spinlock_release(&blk_devices_lock);

    return nullptr;
}

void blk_device_for_each(void (*callback)(const blk_device_t* restrict device, void* restrict arg), void* restrict arg)
{
    for(blk_device_t* it = blk_devices_head; it;) {
        blk_device_t* next = it->priv.next;
        callback(it, arg);
        it = next;
    }
}

int blk_read_sector_prefix(const blk_device_t* restrict device, uintptr_t lba, void* restrict buffer, size_t size)
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

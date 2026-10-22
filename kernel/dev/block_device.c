#include <vex/atomic.h>
#include <vex/block_device.h>
#include <vex/block_event.h>
#include <vex/dev_name.h>
#include <vex/kmalloc.h>
#include <vex/spinlock.h>
#include <vex/string.h>

// Writers also hold block_event_lock, which keeps the list still for
// block_device_for_each; the spinlock is for lookups from anywhere else
static spinlock_t block_devices_lock = SPINLOCK_INIT;
static block_device_t* block_devices_head = nullptr;

static int block_device_register_internal(block_device_t* restrict device)
{
    block_event_lock();
    spinlock_acquire(&block_devices_lock);

    for(block_device_t* it = block_devices_head; it; it = it->priv.next) {
        if(it == device) {
            spinlock_release(&block_devices_lock);
            block_event_unlock();
            return -EEXIST;
        }
    }

    atomic_store(&device->priv.refcount, 1, ATOMIC_RELAXED);

    device->priv.next = block_devices_head;
    block_devices_head = device;

    spinlock_release(&block_devices_lock);

    block_event_notify(BLOCK_EVENT_DEVICE_ADDED, device);

    block_event_unlock();

    return 0;
}

static int name_device(block_device_t* restrict device, const dev_name_t* restrict name)
{
    int retval = dev_name_format(name, device->name, sizeof(device->name));

    if(retval < 0) {
        return retval;
    }

    device->priv.name = *name;
    device->priv.dev = dev_name_to_dev(name);

    return 0;
}

int block_device_add_root(block_device_t* restrict device, dev_unit_t* restrict unit, unsigned index)
{
    dev_name_t name = {};
    name.prefix = unit->prefix;
    name.unit = unit->number;
    name.index = index;
    name.partition = DEV_NAME_NONE;

    int retval = name_device(device, &name);

    if(retval < 0) {
        return retval;
    }

    return block_device_register_internal(device);
}

int block_device_add_part(block_device_t* restrict device, const block_device_t* restrict root, unsigned partition)
{
    dev_name_t name = root->priv.name;
    name.partition = partition;

    int retval = name_device(device, &name);

    if(retval < 0) {
        return retval;
    }

    return block_device_register_internal(device);
}

int block_device_remove(block_device_t* restrict device)
{
    block_event_lock();
    spinlock_acquire(&block_devices_lock);

    block_device_t** prev = &block_devices_head;

    while(*prev && *prev != device) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        spinlock_release(&block_devices_lock);
        block_event_unlock();
        return -ENODEV;
    }

    *prev = device->priv.next;
    device->priv.next = nullptr;

    spinlock_release(&block_devices_lock);

    // Holders of a reference keep the structure, not
    // the hardware; their I/O fails from here on
    bool was_present = device->flags & BLOCK_FLAG_PRESENT;
    device->flags &= ~BLOCK_FLAG_PRESENT;

    if(was_present) {
        block_event_notify(BLOCK_EVENT_MEDIA_DISCONNECTED, device);
    }

    block_event_notify(BLOCK_EVENT_DEVICE_REMOVED, device);

    block_event_unlock();

    block_device_decref(device);

    return 0;
}

void block_device_incref(const block_device_t* restrict device)
{
    block_device_t* mutable_device = (block_device_t*)(device);
    atomic_fetch_add(&mutable_device->priv.refcount, 1, ATOMIC_RELAXED);
}

void block_device_decref(const block_device_t* restrict device)
{
    block_device_t* mutable_device = (block_device_t*)(device);

    if(1 == atomic_fetch_sub(&mutable_device->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        if(device->ops->release) {
            device->ops->release(device);
        }
    }
}

const block_device_t* block_device_find(const char* restrict name)
{
    spinlock_acquire(&block_devices_lock);

    for(block_device_t* it = block_devices_head; it; it = it->priv.next) {
        if(0 == strcmp(it->name, name)) {
            block_device_incref(it);
            spinlock_release(&block_devices_lock);
            return it;
        }
    }

    spinlock_release(&block_devices_lock);

    return nullptr;
}

void block_device_for_each(void (*callback)(const block_device_t* restrict device, void* restrict arg), void* restrict arg)
{
    for(block_device_t* it = block_devices_head; it;) {
        block_device_t* next = it->priv.next;
        callback(it, arg);
        it = next;
    }
}

int block_read_sector_prefix(const block_device_t* restrict device, uintptr_t lba, void* restrict buffer, size_t size)
{
    if(device->sector_size < size) {
        return -EINVAL;
    }

    void* sector = kmalloc(device->sector_size);

    if(sector == nullptr) {
        return -ENOMEM;
    }

    int retval = block_read(device, lba, 1, sector);

    if(retval == 0) {
        memcpy(buffer, sector, size);
    }

    kfree(sector);

    return retval;
}

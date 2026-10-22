#include <vex/atomic.h>
#include <vex/block_event.h>
#include <vex/block_interface.h>
#include <vex/dev_name.h>
#include <vex/kmalloc.h>
#include <vex/spinlock.h>
#include <vex/string.h>

// Writers also hold block_event_lock, which keeps the list still for
// block_interface_for_each; the spinlock is for lookups from anywhere else
static spinlock_t block_interfaces_lock = SPINLOCK_INIT;
static block_interface_t* block_interfaces_head = nullptr;

static int block_interface_register_internal(block_interface_t* restrict interface)
{
    block_event_lock();
    spinlock_acquire(&block_interfaces_lock);

    for(block_interface_t* it = block_interfaces_head; it; it = it->priv.next) {
        if(it == interface) {
            spinlock_release(&block_interfaces_lock);
            block_event_unlock();
            return -EEXIST;
        }
    }

    atomic_store(&interface->priv.refcount, 1, ATOMIC_RELAXED);

    interface->priv.next = block_interfaces_head;
    block_interfaces_head = interface;

    spinlock_release(&block_interfaces_lock);

    block_event_notify(BLOCK_EVENT_INTERFACE_ADDED, interface);

    block_event_unlock();

    return 0;
}

static int name_device(block_interface_t* restrict interface, const dev_name_t* restrict name)
{
    int retval = dev_name_format(name, interface->name, sizeof(interface->name));

    if(retval < 0) {
        return retval;
    }

    interface->priv.name = *name;
    interface->priv.dev = dev_name_to_dev(name);

    return 0;
}

int block_interface_add_root(block_interface_t* restrict interface, dev_unit_t* restrict unit, unsigned index)
{
    dev_name_t name = {};
    name.prefix = unit->prefix;
    name.unit = unit->number;
    name.index = index;
    name.partition = DEV_NAME_NONE;

    int retval = name_device(interface, &name);

    if(retval < 0) {
        return retval;
    }

    return block_interface_register_internal(interface);
}

int block_interface_add_part(block_interface_t* restrict interface, const block_interface_t* restrict root, unsigned partition)
{
    dev_name_t name = root->priv.name;
    name.partition = partition;

    int retval = name_device(interface, &name);

    if(retval < 0) {
        return retval;
    }

    return block_interface_register_internal(interface);
}

int block_interface_remove(block_interface_t* restrict interface)
{
    block_event_lock();
    spinlock_acquire(&block_interfaces_lock);

    block_interface_t** prev = &block_interfaces_head;

    while(*prev && *prev != interface) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        spinlock_release(&block_interfaces_lock);
        block_event_unlock();
        return -ENODEV;
    }

    *prev = interface->priv.next;
    interface->priv.next = nullptr;

    spinlock_release(&block_interfaces_lock);

    // Holders of a reference keep the structure, not
    // the hardware; their I/O fails from here on
    bool was_present = interface->flags & BLOCK_FLAG_PRESENT;
    interface->flags &= ~BLOCK_FLAG_PRESENT;

    if(was_present) {
        block_event_notify(BLOCK_EVENT_MEDIA_DISCONNECTED, interface);
    }

    block_event_notify(BLOCK_EVENT_INTERFACE_REMOVED, interface);

    block_event_unlock();

    block_interface_decref(interface);

    return 0;
}

void block_interface_incref(const block_interface_t* restrict interface)
{
    block_interface_t* mutable_interface = (block_interface_t*)(interface);
    atomic_fetch_add(&mutable_interface->priv.refcount, 1, ATOMIC_RELAXED);
}

void block_interface_decref(const block_interface_t* restrict interface)
{
    block_interface_t* mutable_interface = (block_interface_t*)(interface);

    if(1 == atomic_fetch_sub(&mutable_interface->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        if(interface->ops->release) {
            interface->ops->release(interface);
        }
    }
}

const block_interface_t* block_interface_find(const char* restrict name)
{
    spinlock_acquire(&block_interfaces_lock);

    for(block_interface_t* it = block_interfaces_head; it; it = it->priv.next) {
        if(0 == strcmp(it->name, name)) {
            block_interface_incref(it);
            spinlock_release(&block_interfaces_lock);
            return it;
        }
    }

    spinlock_release(&block_interfaces_lock);

    return nullptr;
}

void block_interface_for_each(void (*callback)(const block_interface_t* restrict interface, void* restrict arg), void* restrict arg)
{
    for(block_interface_t* it = block_interfaces_head; it;) {
        block_interface_t* next = it->priv.next;
        callback(it, arg);
        it = next;
    }
}

int block_read_sector_prefix(const block_interface_t* restrict interface, uintptr_t lba, void* restrict buffer, size_t size)
{
    if(interface->sector_size < size) {
        return -EINVAL;
    }

    void* sector = kmalloc(interface->sector_size);

    if(sector == nullptr) {
        return -ENOMEM;
    }

    int retval = block_read(interface, lba, 1, sector);

    if(retval == 0) {
        memcpy(buffer, sector, size);
    }

    kfree(sector);

    return retval;
}

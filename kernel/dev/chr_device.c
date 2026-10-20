#include <vex/atomic.h>
#include <vex/chr_device.h>
#include <vex/chr_event.h>
#include <vex/chr_name.h>
#include <vex/limits.h>
#include <vex/spinlock.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/strings.h>

// Writers also hold chr_event_lock, which keeps the list still for
// chr_device_for_each; the spinlock is for lookups from anywhere else
static spinlock_t chr_devices_lock = SPINLOCK_INIT;
static chr_device_t* chr_devices_head = nullptr;

static int chr_device_register_internal(chr_device_t* restrict device)
{
    chr_event_lock();
    spinlock_acquire(&chr_devices_lock);

    for(chr_device_t* it = chr_devices_head; it; it = it->priv.next) {
        if(it == device) {
            spinlock_release(&chr_devices_lock);
            chr_event_unlock();
            return -EEXIST;
        }
    }

    atomic_store(&device->priv.refcount, 1, ATOMIC_RELAXED);
    atomic_store(&device->priv.removed, false, ATOMIC_RELAXED);

    device->priv.next = chr_devices_head;
    chr_devices_head = device;

    spinlock_release(&chr_devices_lock);

    chr_event_notify(CHR_EVENT_DEVICE_ADDED, device);

    chr_event_unlock();

    return 0;
}

int chr_device_add(chr_device_t* restrict device, chr_name_t* restrict name)
{
    device->priv.name = name;
    device->priv.name_iterator = chr_name_increment(name);

    snprintf(device->name, sizeof(device->name), "%s%u", name->device_prefix, device->priv.name_iterator);

    return chr_device_register_internal(device);
}

int chr_device_add_named(chr_device_t* restrict device, const char* restrict name)
{
    device->priv.name = nullptr;
    device->priv.name_iterator = 0;

    kstrncpy(device->name, name, sizeof(device->name));

    return chr_device_register_internal(device);
}

int chr_device_remove(chr_device_t* restrict device)
{
    chr_event_lock();
    spinlock_acquire(&chr_devices_lock);

    for(chr_device_t** prev = &chr_devices_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == device) {
            *prev = device->priv.next;
            device->priv.next = nullptr;

            spinlock_release(&chr_devices_lock);

            atomic_store(&device->priv.removed, true, ATOMIC_RELEASE);

            chr_event_notify(CHR_EVENT_DEVICE_REMOVED, device);

            chr_event_unlock();

            chr_device_decref(device);

            return 0;
        }
    }

    spinlock_release(&chr_devices_lock);
    chr_event_unlock();

    return -ENODEV;
}

void chr_device_incref(const chr_device_t* restrict device)
{
    chr_device_t* mutable_device = (chr_device_t*)(device);
    atomic_fetch_add(&mutable_device->priv.refcount, 1, ATOMIC_RELAXED);
}

void chr_device_decref(const chr_device_t* restrict device)
{
    chr_device_t* mutable_device = (chr_device_t*)(device);

    if(1 == atomic_fetch_sub(&mutable_device->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        if(device->ops->release) {
            device->ops->release(device);
        }
    }
}

const chr_device_t* chr_device_find(const char* restrict name)
{
    spinlock_acquire(&chr_devices_lock);

    for(chr_device_t* it = chr_devices_head; it; it = it->priv.next) {
        if(0 == strcmp(it->name, name)) {
            chr_device_incref(it);
            spinlock_release(&chr_devices_lock);
            return it;
        }
    }

    spinlock_release(&chr_devices_lock);

    return nullptr;
}

void chr_device_for_each(void (*callback)(const chr_device_t* restrict device, void* restrict arg), void* restrict arg)
{
    for(chr_device_t* it = chr_devices_head; it;) {
        chr_device_t* next = it->priv.next;
        callback(it, arg);
        it = next;
    }
}

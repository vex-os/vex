#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/chr_device.h>
#include <vex/chr_event.h>
#include <vex/console.h>
#include <vex/dev_name.h>
#include <vex/fcntl.h>
#include <vex/spinlock.h>
#include <vex/string.h>

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

    mutex_init(&device->priv.open_lock);
    device->priv.open_count = 0;
    device->priv.exclusive = false;

    device->priv.next = chr_devices_head;
    chr_devices_head = device;

    spinlock_release(&chr_devices_lock);

    chr_event_notify(CHR_EVENT_DEVICE_ADDED, device);

    chr_event_unlock();

    return 0;
}

int chr_device_add(chr_device_t* restrict device, dev_unit_t* restrict unit, unsigned index)
{
    dev_name_t name = {};
    name.unit = unit;
    name.index = index;
    name.partition = DEV_NAME_NONE;

    int retval = dev_name_format(&name, device->name, sizeof(device->name));

    if(retval < 0) {
        return retval;
    }

    device->priv.name = name;
    device->priv.dev = dev_name_to_dev(&name);

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

            // Waits out an open or close already in the driver
            mutex_acquire(&device->priv.open_lock);
            atomic_store(&device->priv.removed, true, ATOMIC_RELEASE);
            mutex_release(&device->priv.open_lock);

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

void chr_device_revoke_mappings(const chr_device_t* restrict device)
{
    chr_event_lock();
    chr_event_notify(CHR_EVENT_MEMORY_CHANGED, device);
    chr_event_unlock();
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
        mutex_deinit(&mutable_device->priv.open_lock);

        if(device->ops->release) {
            device->ops->release(device);
        }
    }
}

int chr_device_open(const chr_device_t* restrict device, unsigned flags)
{
    chr_device_t* mutable_device = (chr_device_t*)(device);
    int retval = 0;

    mutex_acquire(&mutable_device->priv.open_lock);

    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE)) {
        retval = -ENODEV;
    }
    else if(device->priv.exclusive || (device->priv.open_count > 0 && (flags & O_EXCL))) {
        retval = -EBUSY;
    }
    else if(device->priv.open_count == 0) {
        if(device->console) {
            console_release(device->console);
        }

        if(device->ops->open) {
            retval = device->ops->open(device, flags);
        }
    }

    if(retval >= 0) {
        mutable_device->priv.open_count += 1;
        mutable_device->priv.exclusive = flags & O_EXCL;
        retval = 0;
    }

    mutex_release(&mutable_device->priv.open_lock);

    return retval;
}

void chr_device_close(const chr_device_t* restrict device)
{
    chr_device_t* mutable_device = (chr_device_t*)(device);

    mutex_acquire(&mutable_device->priv.open_lock);

    kassert(device->priv.open_count > 0);
    mutable_device->priv.open_count -= 1;

    if(device->priv.open_count == 0) {
        mutable_device->priv.exclusive = false;
    }

    if(device->priv.open_count == 0 && !atomic_load(&device->priv.removed, ATOMIC_ACQUIRE) && device->ops->close) {
        device->ops->close(device);
    }

    mutex_release(&mutable_device->priv.open_lock);
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

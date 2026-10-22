#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/char_device.h>
#include <vex/char_event.h>
#include <vex/console.h>
#include <vex/dev_name.h>
#include <vex/fcntl.h>
#include <vex/spinlock.h>
#include <vex/string.h>

// Writers also hold char_event_lock, which keeps the list still for
// char_device_for_each; the spinlock is for lookups from anywhere else
static spinlock_t char_devices_lock = SPINLOCK_INIT;
static char_device_t* char_devices_head = nullptr;

static int char_device_register_internal(char_device_t* restrict device)
{
    char_event_lock();
    spinlock_acquire(&char_devices_lock);

    for(char_device_t* it = char_devices_head; it; it = it->priv.next) {
        if(it == device) {
            spinlock_release(&char_devices_lock);
            char_event_unlock();
            return -EEXIST;
        }
    }

    atomic_store(&device->priv.refcount, 1, ATOMIC_RELAXED);
    atomic_store(&device->priv.removed, false, ATOMIC_RELAXED);

    mutex_init(&device->priv.open_lock);
    device->priv.open_count = 0;
    device->priv.exclusive = false;

    device->priv.next = char_devices_head;
    char_devices_head = device;

    spinlock_release(&char_devices_lock);

    char_event_notify(CHAR_EVENT_DEVICE_ADDED, device);

    char_event_unlock();

    return 0;
}

static int add_named(char_device_t* restrict device, const dev_name_t* restrict name)
{
    int retval = dev_name_format(name, device->name, sizeof(device->name));

    if(retval < 0) {
        return retval;
    }

    device->priv.name = *name;
    device->priv.dev = dev_name_to_dev(name);

    return char_device_register_internal(device);
}

int char_device_add(char_device_t* restrict device, dev_unit_t* restrict unit, unsigned index)
{
    dev_name_t name = {};
    name.prefix = unit->prefix;
    name.unit = unit->number;
    name.index = index;
    name.partition = DEV_NAME_NONE;

    return add_named(device, &name);
}

int char_device_add_unitless(char_device_t* restrict device, dev_prefix_t* restrict prefix)
{
    kassert(prefix->claimed);

    dev_name_t name = {};
    name.prefix = prefix;
    name.unit = DEV_NAME_NONE;
    name.index = DEV_NAME_NONE;
    name.partition = DEV_NAME_NONE;

    return add_named(device, &name);
}

int char_device_remove(char_device_t* restrict device)
{
    char_event_lock();
    spinlock_acquire(&char_devices_lock);

    for(char_device_t** prev = &char_devices_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == device) {
            *prev = device->priv.next;
            device->priv.next = nullptr;

            spinlock_release(&char_devices_lock);

            // Waits out an open or close already in the driver
            mutex_acquire(&device->priv.open_lock);
            atomic_store(&device->priv.removed, true, ATOMIC_RELEASE);
            mutex_release(&device->priv.open_lock);

            char_event_notify(CHAR_EVENT_DEVICE_REMOVED, device);

            char_event_unlock();

            char_device_decref(device);

            return 0;
        }
    }

    spinlock_release(&char_devices_lock);
    char_event_unlock();

    return -ENODEV;
}

void char_device_revoke_mappings(const char_device_t* restrict device)
{
    char_event_lock();
    char_event_notify(CHAR_EVENT_MEMORY_CHANGED, device);
    char_event_unlock();
}

void char_device_incref(const char_device_t* restrict device)
{
    char_device_t* mutable_device = (char_device_t*)(device);
    atomic_fetch_add(&mutable_device->priv.refcount, 1, ATOMIC_RELAXED);
}

void char_device_decref(const char_device_t* restrict device)
{
    char_device_t* mutable_device = (char_device_t*)(device);

    if(1 == atomic_fetch_sub(&mutable_device->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        mutex_deinit(&mutable_device->priv.open_lock);

        if(device->ops->release) {
            device->ops->release(device);
        }
    }
}

int char_device_open(const char_device_t* restrict device, unsigned flags, void** restrict file_ctxdata)
{
    char_device_t* mutable_device = (char_device_t*)(device);
    int retval = 0;

    mutex_acquire(&mutable_device->priv.open_lock);

    bool first = device->priv.open_count == 0;

    if(atomic_load(&device->priv.removed, ATOMIC_ACQUIRE)) {
        retval = -ENODEV;
    }
    else if(device->priv.exclusive || (!first && (flags & O_EXCL))) {
        retval = -EBUSY;
    }
    else if(first) {
        if(device->console) {
            console_release(device->console);
        }

        if(device->ops->open) {
            retval = device->ops->open(device, flags);
        }
    }

    if(retval >= 0 && device->ops->open_file) {
        retval = device->ops->open_file(device, flags, file_ctxdata);

        if(retval < 0 && first && device->ops->close) {
            device->ops->close(device);
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

void char_device_set_exclusive(const char_device_t* restrict device, bool exclusive)
{
    char_device_t* mutable_device = (char_device_t*)(device);

    mutex_acquire(&mutable_device->priv.open_lock);
    mutable_device->priv.exclusive = exclusive;
    mutex_release(&mutable_device->priv.open_lock);
}

bool char_device_is_exclusive(const char_device_t* restrict device)
{
    char_device_t* mutable_device = (char_device_t*)(device);

    mutex_acquire(&mutable_device->priv.open_lock);
    bool exclusive = device->priv.exclusive;
    mutex_release(&mutable_device->priv.open_lock);

    return exclusive;
}

int char_device_refuse_opens(const char_device_t* restrict device)
{
    char_device_t* mutable_device = (char_device_t*)(device);
    int retval = 0;

    mutex_acquire(&mutable_device->priv.open_lock);

    if(device->priv.open_count) {
        retval = -EBUSY;
    }
    else {
        mutable_device->priv.exclusive = true;
    }

    mutex_release(&mutable_device->priv.open_lock);

    return retval;
}

void char_device_close(const char_device_t* restrict device, void* restrict file_ctxdata)
{
    char_device_t* mutable_device = (char_device_t*)(device);

    mutex_acquire(&mutable_device->priv.open_lock);

    if(device->ops->close_file) {
        device->ops->close_file(device, file_ctxdata);
    }

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

const char_device_t* char_device_find(const char* restrict name)
{
    spinlock_acquire(&char_devices_lock);

    for(char_device_t* it = char_devices_head; it; it = it->priv.next) {
        if(0 == strcmp(it->name, name)) {
            char_device_incref(it);
            spinlock_release(&char_devices_lock);
            return it;
        }
    }

    spinlock_release(&char_devices_lock);

    return nullptr;
}

void char_device_for_each(void (*callback)(const char_device_t* restrict device, void* restrict arg), void* restrict arg)
{
    for(char_device_t* it = char_devices_head; it;) {
        char_device_t* next = it->priv.next;
        callback(it, arg);
        it = next;
    }
}

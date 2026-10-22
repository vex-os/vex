#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/char_event.h>
#include <vex/char_interface.h>
#include <vex/console.h>
#include <vex/dev_name.h>
#include <vex/devctl.h>
#include <vex/fcntl.h>
#include <vex/spinlock.h>
#include <vex/string.h>

// Writers also hold char_event_lock, which keeps the list still for
// char_interface_for_each; the spinlock is for lookups from anywhere else
static spinlock_t char_interfaces_lock = SPINLOCK_INIT;
static char_interface_t* char_interfaces_head = nullptr;

static int char_interface_register_internal(char_interface_t* restrict interface)
{
    char_event_lock();
    spinlock_acquire(&char_interfaces_lock);

    for(char_interface_t* it = char_interfaces_head; it; it = it->priv.next) {
        if(it == interface) {
            spinlock_release(&char_interfaces_lock);
            char_event_unlock();
            return -EEXIST;
        }
    }

    atomic_store(&interface->priv.refcount, 1, ATOMIC_RELAXED);
    atomic_store(&interface->priv.removed, false, ATOMIC_RELAXED);

    mutex_init(&interface->priv.open_lock);
    interface->priv.open_count = 0;
    interface->priv.exclusive = false;

    interface->priv.next = char_interfaces_head;
    char_interfaces_head = interface;

    spinlock_release(&char_interfaces_lock);

    char_event_notify(CHAR_EVENT_INTERFACE_ADDED, interface);

    if(interface->device) {
        devctl_interface_added(interface->device, "char", interface->name);
    }

    char_event_unlock();

    return 0;
}

static int add_named(char_interface_t* restrict interface, const dev_name_t* restrict name)
{
    int retval = dev_name_format(name, interface->name, sizeof(interface->name));

    if(retval < 0) {
        return retval;
    }

    interface->priv.name = *name;
    interface->priv.dev = dev_name_to_dev(name);

    return char_interface_register_internal(interface);
}

int char_interface_add(char_interface_t* restrict interface, dev_unit_t* restrict unit, unsigned index)
{
    dev_name_t name = {};
    name.prefix = unit->prefix;
    name.unit = unit->number;
    name.index = index;
    name.partition = DEV_NAME_NONE;

    return add_named(interface, &name);
}

int char_interface_add_unitless(char_interface_t* restrict interface, dev_prefix_t* restrict prefix)
{
    kassert(prefix->claimed);

    dev_name_t name = {};
    name.prefix = prefix;
    name.unit = DEV_NAME_NONE;
    name.index = DEV_NAME_NONE;
    name.partition = DEV_NAME_NONE;

    return add_named(interface, &name);
}

int char_interface_remove(char_interface_t* restrict interface)
{
    char_event_lock();
    spinlock_acquire(&char_interfaces_lock);

    for(char_interface_t** prev = &char_interfaces_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == interface) {
            *prev = interface->priv.next;
            interface->priv.next = nullptr;

            spinlock_release(&char_interfaces_lock);

            // Waits out an open or close already in the driver
            mutex_acquire(&interface->priv.open_lock);
            atomic_store(&interface->priv.removed, true, ATOMIC_RELEASE);
            mutex_release(&interface->priv.open_lock);

            char_event_notify(CHAR_EVENT_INTERFACE_REMOVED, interface);

            if(interface->device) {
                devctl_interface_removed(interface->device, interface->name);
            }

            char_event_unlock();

            char_interface_decref(interface);

            return 0;
        }
    }

    spinlock_release(&char_interfaces_lock);
    char_event_unlock();

    return -ENODEV;
}

void char_interface_revoke_mappings(const char_interface_t* restrict interface)
{
    char_event_lock();
    char_event_notify(CHAR_EVENT_MEMORY_CHANGED, interface);
    char_event_unlock();
}

void char_interface_incref(const char_interface_t* restrict interface)
{
    char_interface_t* mutable_interface = (char_interface_t*)(interface);
    atomic_fetch_add(&mutable_interface->priv.refcount, 1, ATOMIC_RELAXED);
}

void char_interface_decref(const char_interface_t* restrict interface)
{
    char_interface_t* mutable_interface = (char_interface_t*)(interface);

    if(1 == atomic_fetch_sub(&mutable_interface->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        mutex_deinit(&mutable_interface->priv.open_lock);

        if(interface->ops->release) {
            interface->ops->release(interface);
        }
    }
}

int char_interface_open(const char_interface_t* restrict interface, unsigned flags, void** restrict file_ctxdata)
{
    char_interface_t* mutable_interface = (char_interface_t*)(interface);
    int retval = 0;

    mutex_acquire(&mutable_interface->priv.open_lock);

    bool first = interface->priv.open_count == 0;

    if(atomic_load(&interface->priv.removed, ATOMIC_ACQUIRE)) {
        retval = -ENODEV;
    }
    else if(interface->priv.exclusive || (!first && (flags & O_EXCL))) {
        retval = -EBUSY;
    }
    else if(first) {
        if(interface->console) {
            console_release(interface->console);
        }

        if(interface->ops->open) {
            retval = interface->ops->open(interface, flags);
        }
    }

    if(retval >= 0 && interface->ops->open_file) {
        retval = interface->ops->open_file(interface, flags, file_ctxdata);

        if(retval < 0 && first && interface->ops->close) {
            interface->ops->close(interface);
        }
    }

    if(retval >= 0) {
        mutable_interface->priv.open_count += 1;
        mutable_interface->priv.exclusive = flags & O_EXCL;
        retval = 0;
    }

    mutex_release(&mutable_interface->priv.open_lock);

    return retval;
}

void char_interface_set_exclusive(const char_interface_t* restrict interface, bool exclusive)
{
    char_interface_t* mutable_interface = (char_interface_t*)(interface);

    mutex_acquire(&mutable_interface->priv.open_lock);
    mutable_interface->priv.exclusive = exclusive;
    mutex_release(&mutable_interface->priv.open_lock);
}

bool char_interface_is_exclusive(const char_interface_t* restrict interface)
{
    char_interface_t* mutable_interface = (char_interface_t*)(interface);

    mutex_acquire(&mutable_interface->priv.open_lock);
    bool exclusive = interface->priv.exclusive;
    mutex_release(&mutable_interface->priv.open_lock);

    return exclusive;
}

int char_interface_refuse_opens(const char_interface_t* restrict interface)
{
    char_interface_t* mutable_interface = (char_interface_t*)(interface);
    int retval = 0;

    mutex_acquire(&mutable_interface->priv.open_lock);

    if(interface->priv.open_count) {
        retval = -EBUSY;
    }
    else {
        mutable_interface->priv.exclusive = true;
    }

    mutex_release(&mutable_interface->priv.open_lock);

    return retval;
}

void char_interface_close(const char_interface_t* restrict interface, void* restrict file_ctxdata)
{
    char_interface_t* mutable_interface = (char_interface_t*)(interface);

    mutex_acquire(&mutable_interface->priv.open_lock);

    if(interface->ops->close_file) {
        interface->ops->close_file(interface, file_ctxdata);
    }

    kassert(interface->priv.open_count > 0);
    mutable_interface->priv.open_count -= 1;

    if(interface->priv.open_count == 0) {
        mutable_interface->priv.exclusive = false;
    }

    if(interface->priv.open_count == 0 && !atomic_load(&interface->priv.removed, ATOMIC_ACQUIRE) && interface->ops->close) {
        interface->ops->close(interface);
    }

    mutex_release(&mutable_interface->priv.open_lock);
}

const char_interface_t* char_interface_find(const char* restrict name)
{
    spinlock_acquire(&char_interfaces_lock);

    for(char_interface_t* it = char_interfaces_head; it; it = it->priv.next) {
        if(0 == strcmp(it->name, name)) {
            char_interface_incref(it);
            spinlock_release(&char_interfaces_lock);
            return it;
        }
    }

    spinlock_release(&char_interfaces_lock);

    return nullptr;
}

void char_interface_for_each(void (*callback)(const char_interface_t* restrict interface, void* restrict arg), void* restrict arg)
{
    for(char_interface_t* it = char_interfaces_head; it;) {
        char_interface_t* next = it->priv.next;
        callback(it, arg);
        it = next;
    }
}

#include <vex/chr_device.h>
#include <vex/chr_event.h>
#include <vex/chr_name.h>
#include <vex/limits.h>
#include <vex/spinlock.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/strings.h>

static spinlock_type chr_devices_lock = SPINLOCK_INIT;
static struct chr_device* chr_devices_head = nullptr;

static int chr_device_register_internal(struct chr_device* restrict device)
{
    chr_event_lock();
    spinlock_acquire(&chr_devices_lock);

    for(struct chr_device* it = chr_devices_head; it; it = it->priv.next) {
        if(it == device) {
            spinlock_release(&chr_devices_lock);
            chr_event_unlock();
            return -EEXIST;
        }
    }

    device->priv.next = chr_devices_head;
    chr_devices_head = device;

    spinlock_release(&chr_devices_lock);

    chr_event_notify(CHR_EVENT_DEVICE_ADDED, device);

    chr_event_unlock();

    return 0;
}

int add_chr_device(struct chr_device* restrict device, struct chr_name* restrict name)
{
    device->priv.name = name;
    device->priv.name_iterator = chr_name_increment(name);

    snprintf(device->identifier, sizeof(device->identifier), "%s%u", name->device_prefix, device->priv.name_iterator);

    return chr_device_register_internal(device);
}

int add_chr_device_named(struct chr_device* restrict device, const char* restrict name)
{
    device->priv.name = nullptr;
    device->priv.name_iterator = 0;

    kstrncpy(device->identifier, name, sizeof(device->identifier));

    return chr_device_register_internal(device);
}

int remove_chr_device(struct chr_device* restrict device)
{
    chr_event_lock();
    spinlock_acquire(&chr_devices_lock);

    for(struct chr_device** prev = &chr_devices_head; *prev; prev = &(*prev)->priv.next) {
        if(*prev == device) {
            *prev = device->priv.next;
            device->priv.next = nullptr;

            spinlock_release(&chr_devices_lock);

            chr_event_notify(CHR_EVENT_DEVICE_REMOVED, device);

            chr_event_unlock();

            return 0;
        }
    }

    spinlock_release(&chr_devices_lock);
    chr_event_unlock();

    return -ENODEV;
}

const struct chr_device* find_chr_device(const char* restrict name)
{
    spinlock_acquire(&chr_devices_lock);

    for(struct chr_device* it = chr_devices_head; it; it = it->priv.next) {
        if(0 == strcmp(it->identifier, name)) {
            spinlock_release(&chr_devices_lock);
            return it;
        }
    }

    spinlock_release(&chr_devices_lock);

    return nullptr;
}

void iterate_chr_devices(void (*callback)(const struct chr_device* restrict device, void* restrict arg), void* restrict arg)
{
    for(struct chr_device* it = chr_devices_head; it;) {
        struct chr_device* next = it->priv.next;
        callback(it, arg);
        it = next;
    }
}

#include <dev/chr_device.h>
#include <dev/chr_event.h>
#include <dev/chr_name.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <limits.h>

static struct chr_device* chr_device_head = nullptr;

static int chr_device_register_internal(struct chr_device* restrict device)
{
    struct chr_device* it;

    for(it = chr_device_head; it; it = it->priv.next) {
        if(it == device) {
            return -EEXIST;
        }
    }

    device->priv.next = chr_device_head;
    chr_device_head = device;

    chr_event_notify(CHR_EVENT_DEVICE_ADDED, device);

    return 0;
}

int chr_device_register(struct chr_device* restrict device, struct chr_name* restrict name)
{
    device->priv.name = name;
    device->priv.name_iterator = 0;

    snprintf(device->identifier, sizeof(device->identifier), "%s%u", name->device_prefix, name->priv.next_iterator);

    return chr_device_register_internal(device);
}

int chr_device_register_custom(struct chr_device* restrict device, const char* restrict name)
{
    device->priv.name = nullptr;
    device->priv.name_iterator = 0;

    kstrncpy(device->identifier, name, sizeof(device->identifier));

    return chr_device_register_internal(device);
}

int chr_device_unregister(struct chr_device* restrict device)
{
    struct chr_device* it;
    struct chr_device* prev = nullptr;

    for(it = chr_device_head; it; prev = it, it = it->priv.next) {
        if(it == device) {
            if(prev) {
                prev->priv.next = it->priv.next;
            }
            else {
                chr_device_head = it->priv.next;
            }

            chr_event_notify(CHR_EVENT_DEVICE_REMOVED, device);

            return 0;
        }
    }

    return -ENODEV;
}

const struct chr_device* chr_device_find(const char* restrict name)
{
    struct chr_device* it;

    for(it = chr_device_head; it; it = it->priv.next) {
        if(strcmp(it->identifier, name) == 0) {
            return it;
        }
    }

    return nullptr;
}

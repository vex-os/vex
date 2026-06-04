#include <dev/blk_device.h>
#include <dev/blk_event.h>
#include <dev/blk_name.h>
#include <lib/sprintf.h>
#include <lib/string.h>
#include <limits.h>

struct blk_device* blk_devices_head = nullptr;

static int blk_device_register_internal(struct blk_device* restrict device)
{
    struct blk_device* it;

    for(it = blk_devices_head; it; it = it->priv.next) {
        if(it == device) {
            return -EEXIST;
        }
    }

    device->priv.next = blk_devices_head;
    blk_devices_head = device;

    blk_event_notify(BLK_EVENT_DEVICE_ADDED, device);

    return 0;
}

int blk_device_register_root(struct blk_device* restrict device, struct blk_name* restrict name, unsigned iterator, unsigned index)
{
    device->priv.name = name;
    device->priv.name_iterator = iterator;
    device->partition = UINT_MAX;

    snprintf(device->identifier, sizeof(device->identifier), "%s%un%u", name->device_prefix, device->priv.name_iterator, index + 1);

    return blk_device_register_internal(device);
}

int blk_device_register_part(struct blk_device* restrict device, const struct blk_device* restrict root, unsigned partition)
{
    device->priv.name = root->priv.name;
    device->priv.name_iterator = root->priv.name_iterator;
    device->partition = partition;

    snprintf(device->identifier, sizeof(device->identifier), "%sp%u", root->identifier, partition + 1);

    return blk_device_register_internal(device);
}

int blk_device_register_custom(struct blk_device* restrict device)
{
    device->priv.name = nullptr;
    device->priv.name_iterator = 0;
    device->partition = UINT_MAX;
    return blk_device_register_internal(device);
}

int blk_device_unregister(struct blk_device* restrict device)
{
    struct blk_device* it;
    struct blk_device* prev = nullptr;

    if(blk_devices_head == device) {
        blk_devices_head = device->priv.next;
    }
    else {
        for(it = blk_devices_head; it; it = it->priv.next) {
            if(it == device) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return -ENODEV;
        }
    }

    if(device->flags & BLK_FLAG_PRESENT) {
        blk_event_notify(BLK_EVENT_MEDIA_DISCONNECTED, device);
    }

    blk_event_notify(BLK_EVENT_DEVICE_REMOVED, device);

    if(device->ops->destroy) {
        device->ops->destroy(device);
    }

    return 0;
}

const struct blk_device* blk_device_find(const char* restrict name)
{
    struct blk_device* it;

    for(it = blk_devices_head; it; it = it->priv.next) {
        if(strcmp(it->identifier, name) == 0) {
            return it;
        }
    }

    return nullptr;
}

#include <core/kprintf.h>
#include <dev/blk_device.h>
#include <dev/blk_name.h>
#include <lib/sprintf.h>
#include <lib/string.h>

static struct blk_device* blk_device_head = nullptr;

static int blk_device_register_internal(struct blk_device* restrict device)
{
    struct blk_device* it;

    for(it = blk_device_head; it; it = it->priv.next) {
        if(it == device) {
            return EEXIST;
        }
    }

    pr_debug("blk_device: adding %s: sector_count=%zu, of sector_size=%zu", device->identifier, device->sector_count, device->sector_size);

    device->priv.next = blk_device_head;
    blk_device_head = device;

    return 0;
}

int blk_device_register(struct blk_device* restrict device, struct blk_name* restrict name, unsigned index)
{
    device->priv.name = name;
    device->priv.name_iterator = blk_name_increment(name);
    blk_name_format(device->identifier, sizeof(device->identifier), name, device->priv.name_iterator, index);
    return blk_device_register_internal(device);
}

int blk_device_register_custom(struct blk_device* restrict device)
{
    device->priv.name = nullptr;
    device->priv.name_iterator = 0;
    return blk_device_register_internal(device);
}

int blk_device_unregister(struct blk_device* restrict device)
{
    struct blk_device* it;
    struct blk_device* prev = nullptr;

    if(blk_device_head == device) {
        blk_device_head = device->priv.next;
    }
    else {
        for(it = blk_device_head; it; it = it->priv.next) {
            if(it == device) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return ENODEV;
        }
    }

    pr_debug("blk_device: removing %s", device->identifier);

    if(device->ops->destroy) {
        device->ops->destroy(device);
    }

    return 0;
}

const struct blk_device* blk_device_find(const char* name)
{
    struct blk_device* it;

    for(it = blk_device_head; it; it = it->priv.next) {
        if(strcmp(it->identifier, name) == 0) {
            return it;
        }
    }

    return nullptr;
}

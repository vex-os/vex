#include <core/errno.h>
#include <core/kprintf.h>
#include <dev/block_device.h>
#include <lib/string.h>

static struct block_device* block_device_head = nullptr;

int block_device_register(struct block_device* restrict device)
{
    struct block_device* it;

    for(it = block_device_head; it; it = it->priv.next) {
        if(it == device) {
            return EEXIST;
        }
    }

    pr_debug("block_device: adding %s: sector_count=%zu, of sector_size=%zu", device->identifier, device->sector_count,
        device->sector_size);

    device->priv.next = block_device_head;
    block_device_head = device;

    return 0;
}

int block_device_unregister(struct block_device* restrict device)
{
    struct block_device* it;
    struct block_device* prev = nullptr;

    if(block_device_head == device) {
        block_device_head = device->priv.next;
    }
    else {
        for(it = block_device_head; it; it = it->priv.next) {
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

    pr_debug("block_device: removing %s", device->identifier);

    if(device->ops->destroy) {
        device->ops->destroy(device);
    }

    return 0;
}

struct block_device* block_device_find(const char* name)
{
    struct block_device* it;

    for(it = block_device_head; it; it = it->priv.next) {
        if(0 == strcmp(it->identifier, name)) {
            return it;
        }
    }

    return nullptr;
}

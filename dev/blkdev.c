#include <core/errno.h>
#include <core/kprintf.h>
#include <dev/blkdev.h>
#include <lib/string.h>

static struct blkdev* blkdev_head = nullptr;

int register_blkdev(struct blkdev* restrict device)
{
    struct blkdev* it;

    for(it = blkdev_head; it; it = it->priv.next) {
        if(it == device) {
            return EEXIST;
        }
    }

    pr_debug("blkdev: adding %s: sector_cnt=%zu, of sector_sz=%zu", device->identifier, device->sector_cnt, device->sector_sz);

    device->priv.next = blkdev_head;
    blkdev_head = device;

    return 0;
}

int unregister_blkdev(struct blkdev* restrict device)
{
    struct blkdev* it;
    struct blkdev* prev = nullptr;

    if(blkdev_head == device) {
        blkdev_head = device->priv.next;
    }
    else {
        for(it = blkdev_head; it; it = it->priv.next) {
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

    pr_debug("blkdev: removing %s", device->identifier);

    if(device->ops->destroy) {
        device->ops->destroy(device);
    }

    return 0;
}

struct blkdev* blkdev_find(const char* name)
{
    struct blkdev* it;

    for(it = blkdev_head; it; it = it->priv.next) {
        if(0 == strcmp(it->identifier, name)) {
            return it;
        }
    }

    return nullptr;
}

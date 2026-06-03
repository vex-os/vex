#include <core/errno.h>
#include <core/kprintf.h>
#include <dev/blk_device.h>
#include <dev/blk_event.h>
#include <lib/hrsize.h>
#include <mm/malloc.h>

struct event_handler {
    struct event_handler* next;
    blk_event_handler_type handler;
    void* ctxdata;
};

static struct event_handler* handlers_head = nullptr;

#if BLK_EVENT_LOGGING
static void log_blk_event(unsigned event, const struct blk_device* restrict device)
{
    char hrsize_buffer[256];

    switch(event) {
        case BLK_EVENT_DEVICE_ADDED:
            pr_debug("blk_event: device added: %s", device->identifier);
            break;

        case BLK_EVENT_DEVICE_REMOVED:
            pr_debug("blk_event: device removed: %s", device->identifier);
            break;

        case BLK_EVENT_MEDIA_CONNECTED:
            hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), device->sector_count * device->sector_size);
            pr_debug("blk_event: media connected: %s: %s [%zu/%zu]", device->identifier, hrsize_buffer, device->sector_count,
                device->sector_size);
            break;

        case BLK_EVENT_MEDIA_DISCONNECTED:
            pr_debug("blk_event: media disconnected: %s", device->identifier);
            break;

        case BLK_EVENT_PARTITION_ADDED:
            hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), device->sector_count * device->sector_size);
            pr_debug("blk_event: partition added: %s: %s [%zu/%zu]", device->identifier, hrsize_buffer, device->sector_count,
                device->sector_size);
            break;

        case BLK_EVENT_PARTITION_REMOVED:
            pr_debug("blk_event: partition removed: %s", device->identifier);
            break;
    }
}
#endif

int blk_event_register_handler(blk_event_handler_type handler, void* restrict ctxdata)
{
    struct event_handler* it;

    for(it = handlers_head; it; it = it->next) {
        if(it->handler == handler && it->ctxdata == ctxdata) {
            return EEXIST;
        }
    }

    it = kmalloc(sizeof(struct event_handler));

    if(it == nullptr) {
        return ENOMEM;
    }

    it->handler = handler;
    it->ctxdata = ctxdata;

    it->next = handlers_head;
    handlers_head = it;

    return 0;
}

int blk_event_unregister_handler(blk_event_handler_type handler, void* restrict ctxdata)
{
    struct event_handler* it;
    struct event_handler* prev = nullptr;

    for(it = handlers_head; it; it = it->next) {
        if(it->handler == handler && it->ctxdata == ctxdata) {
            if(prev) {
                prev->next = it->next;
            }
            else {
                handlers_head = it->next;
            }

            kfree(it);

            return 0;
        }

        prev = it;
    }

    return ENOENT;
}

void blk_event_notify(unsigned event, const struct blk_device* restrict device)
{
    struct event_handler* it;

#if BLK_EVENT_LOGGING
    log_blk_event(event, device);
#endif

    for(it = handlers_head; it; it = it->next) {
        it->handler(event, device, it->ctxdata);
    }
}

#include <core/errno.h>
#include <core/kprintf.h>
#include <dev/blk_device.h>
#include <dev/blk_event.h>
#include <mm/malloc.h>

struct event_handler {
    struct event_handler* next;
    blk_event_handler_type handler;
    void* ctxdata;
};

static struct event_handler* handlers_head = nullptr;

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

    switch(event) {
        case BLK_EVENT_REGISTERED:
            pr_debug("blk_event: registered %s", device->identifier);
            break;

        case BLK_EVENT_UNREGISTERED:
            pr_debug("blk_event: unregistered %s", device->identifier);
            break;

        case BLK_EVENT_CONNECTED:
            pr_inform("blk_event: connected %s: sector_count=%zu sector_size=%zu", device->identifier, device->sector_count,
                device->sector_size);
            break;

        case BLK_EVENT_DISCONNECTED:
            pr_inform("blk_event: disconnected %s", device->identifier);
            break;
    }

    for(it = handlers_head; it; it = it->next) {
        it->handler(event, device, it->ctxdata);
    }
}

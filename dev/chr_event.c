#include <core/errno.h>
#include <core/kprintf.h>
#include <dev/chr_device.h>
#include <dev/chr_event.h>
#include <mm/malloc.h>

struct event_handler {
    struct event_handler* next;
    chr_event_handler_type handler;
    void* ctxdata;
};

static struct event_handler* handlers_head = nullptr;

#if CHR_EVENT_LOGGING
static void log_chr_event(unsigned event, const struct chr_device* restrict device)
{
    switch(event) {
        case CHR_EVENT_DEVICE_ADDED:
            pr_debug("chr_event: device added: %s", device->identifier);
            break;

        case CHR_EVENT_DEVICE_REMOVED:
            pr_debug("chr_event: device removed: %s", device->identifier);
            break;
    }
}
#endif

int chr_event_register_handler(chr_event_handler_type handler, void* restrict ctxdata)
{
    struct event_handler* it;

    for(it = handlers_head; it; it = it->next) {
        if(it->handler == handler && it->ctxdata == ctxdata) {
            return -EEXIST;
        }
    }

    it = kmalloc(sizeof(struct event_handler));

    if(it == nullptr) {
        return -ENOMEM;
    }

    it->handler = handler;
    it->ctxdata = ctxdata;

    it->next = handlers_head;
    handlers_head = it;

    return 0;
}

int chr_event_unregister_handler(chr_event_handler_type handler, void* restrict ctxdata)
{
    struct event_handler* it;
    struct event_handler* prev = nullptr;

    for(it = handlers_head; it; prev = it, it = it->next) {
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
    }

    return -ENOENT;
}

void chr_event_notify(unsigned event, const struct chr_device* restrict device)
{
    struct event_handler* it;

#if CHR_EVENT_LOGGING
    log_chr_event(event, device);
#endif

    for(it = handlers_head; it; it = it->next) {
        it->handler(event, device, it->ctxdata);
    }
}

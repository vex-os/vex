#include <vex/chr_device.h>
#include <vex/chr_event.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>

struct event_handler {
    struct event_handler* next;
    chr_event_handler_type handler;
    void* ctxdata;
};

static struct mutex events_mutex = MUTEX_INIT;
static unsigned events_depth = 0;
static struct event_handler* handlers_head = nullptr;

#ifdef CONFIG_TRACE_CHR_EVENTS
static void log_chr_event(unsigned event, const struct chr_device* restrict device)
{
    switch(event) {
        case CHR_EVENT_DEVICE_ADDED:
            pr_verbose("chr_event: device added: %s", device->identifier);
            break;

        case CHR_EVENT_DEVICE_REMOVED:
            pr_verbose("chr_event: device removed: %s", device->identifier);
            break;
    }
}
#endif

static void replay_device(const struct chr_device* restrict device, void* restrict arg)
{
    const struct event_handler* entry = arg;
    entry->handler(CHR_EVENT_DEVICE_ADDED, device, entry->ctxdata);
}

void chr_event_lock(void)
{
    if(mutex_held(&events_mutex)) {
        events_depth += 1;
        return;
    }

    mutex_acquire(&events_mutex);
}

void chr_event_unlock(void)
{
    if(events_depth > 0) {
        events_depth -= 1;
        return;
    }

    mutex_release(&events_mutex);
}

int chr_event_subscribe(chr_event_handler_type handler, void* restrict ctxdata)
{
    if(mutex_held(&events_mutex)) {
        return -EDEADLK;
    }

    struct event_handler* entry = kmalloc(sizeof(struct event_handler));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    entry->handler = handler;
    entry->ctxdata = ctxdata;

    mutex_acquire(&events_mutex);

    for(struct event_handler* it = handlers_head; it; it = it->next) {
        if(it->handler == handler && it->ctxdata == ctxdata) {
            mutex_release(&events_mutex);
            kfree(entry);
            return -EEXIST;
        }
    }

    entry->next = handlers_head;
    handlers_head = entry;

    iterate_chr_devices(&replay_device, entry);

    mutex_release(&events_mutex);

    return 0;
}

int chr_event_unsubscribe(chr_event_handler_type handler, void* restrict ctxdata)
{
    if(mutex_held(&events_mutex)) {
        return -EDEADLK;
    }

    mutex_acquire(&events_mutex);

    for(struct event_handler** prev = &handlers_head; *prev; prev = &(*prev)->next) {
        struct event_handler* it = *prev;

        if(it->handler == handler && it->ctxdata == ctxdata) {
            *prev = it->next;
            mutex_release(&events_mutex);
            kfree(it);
            return 0;
        }
    }

    mutex_release(&events_mutex);

    return -ENOENT;
}

void chr_event_notify(unsigned event, const struct chr_device* restrict device)
{
#ifdef CONFIG_TRACE_CHR_EVENTS
    log_chr_event(event, device);
#endif

    chr_event_lock();

    for(struct event_handler* it = handlers_head; it; it = it->next) {
        it->handler(event, device, it->ctxdata);
    }

    chr_event_unlock();
}

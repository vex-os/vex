#include <vex/chr_device.h>
#include <vex/chr_event.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>

typedef struct event_handler_s event_handler_t;

struct event_handler_s {
    event_handler_t* next;
    chr_event_handler_t handler;
    void* ctxdata;
};

static mutex_t events_mutex = MUTEX_INIT;
static unsigned events_depth = 0;
static event_handler_t* handlers_head = nullptr;

#ifdef CONFIG_TRACE_CHR_EVENTS
static void log_chr_event(unsigned event, const chr_device_t* restrict device)
{
    switch(event) {
        case CHR_EVENT_DEVICE_ADDED:
            pr_verbose("chr_event: device added: %s", device->name);
            break;

        case CHR_EVENT_DEVICE_REMOVED:
            pr_verbose("chr_event: device removed: %s", device->name);
            break;
    }
}
#endif

static void replay_device(const chr_device_t* restrict device, void* restrict arg)
{
    const event_handler_t* entry = arg;
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

int chr_event_subscribe(chr_event_handler_t handler, void* restrict ctxdata)
{
    if(mutex_held(&events_mutex)) {
        return -EDEADLK;
    }

    event_handler_t* entry = kmalloc(sizeof(event_handler_t));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    entry->handler = handler;
    entry->ctxdata = ctxdata;

    mutex_acquire(&events_mutex);

    for(event_handler_t* it = handlers_head; it; it = it->next) {
        if(it->handler == handler && it->ctxdata == ctxdata) {
            mutex_release(&events_mutex);
            kfree(entry);
            return -EEXIST;
        }
    }

    entry->next = handlers_head;
    handlers_head = entry;

    chr_device_for_each(&replay_device, entry);

    mutex_release(&events_mutex);

    return 0;
}

int chr_event_unsubscribe(chr_event_handler_t handler, void* restrict ctxdata)
{
    if(mutex_held(&events_mutex)) {
        return -EDEADLK;
    }

    mutex_acquire(&events_mutex);

    for(event_handler_t** prev = &handlers_head; *prev; prev = &(*prev)->next) {
        event_handler_t* it = *prev;

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

void chr_event_notify(unsigned event, const chr_device_t* restrict device)
{
#ifdef CONFIG_TRACE_CHR_EVENTS
    log_chr_event(event, device);
#endif

    chr_event_lock();

    for(event_handler_t* it = handlers_head; it; it = it->next) {
        it->handler(event, device, it->ctxdata);
    }

    chr_event_unlock();
}

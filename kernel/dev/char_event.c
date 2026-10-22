#include <vex/char_event.h>
#include <vex/char_interface.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>

typedef struct event_handler_s event_handler_t;

struct event_handler_s {
    event_handler_t* next;
    char_event_handler_t handler;
    void* ctxdata;
};

static mutex_t events_mutex = MUTEX_INIT;
static unsigned events_depth = 0;
static event_handler_t* handlers_head = nullptr;

#ifdef CONFIG_TRACE_CHR_EVENTS
static void log_char_event(unsigned event, const char_interface_t* restrict interface)
{
    switch(event) {
        case CHAR_EVENT_INTERFACE_ADDED:
            pr_verbose("char_event: device added: %s", interface->name);
            break;

        case CHAR_EVENT_INTERFACE_REMOVED:
            pr_verbose("char_event: device removed: %s", interface->name);
            break;
    }
}
#endif

static void replay_device(const char_interface_t* restrict interface, void* restrict arg)
{
    const event_handler_t* entry = arg;
    entry->handler(CHAR_EVENT_INTERFACE_ADDED, interface, entry->ctxdata);
}

void char_event_lock(void)
{
    if(mutex_held(&events_mutex)) {
        events_depth += 1;
        return;
    }

    mutex_acquire(&events_mutex);
}

void char_event_unlock(void)
{
    if(events_depth > 0) {
        events_depth -= 1;
        return;
    }

    mutex_release(&events_mutex);
}

int char_event_subscribe(char_event_handler_t handler, void* restrict ctxdata)
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

    char_interface_for_each(&replay_device, entry);

    mutex_release(&events_mutex);

    return 0;
}

int char_event_unsubscribe(char_event_handler_t handler, void* restrict ctxdata)
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

void char_event_notify(unsigned event, const char_interface_t* restrict interface)
{
#ifdef CONFIG_TRACE_CHR_EVENTS
    log_char_event(event, interface);
#endif

    char_event_lock();

    for(event_handler_t* it = handlers_head; it; it = it->next) {
        it->handler(event, interface, it->ctxdata);
    }

    char_event_unlock();
}

#include <vex/block_event.h>
#include <vex/block_interface.h>
#include <vex/errno.h>
#include <vex/hrsize.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>

typedef struct event_handler_s event_handler_t;

struct event_handler_s {
    event_handler_t* next;
    block_event_handler_t handler;
    void* ctxdata;
};

static mutex_t events_mutex = MUTEX_INIT;
static unsigned events_depth = 0;
static event_handler_t* handlers_head = nullptr;

#ifdef CONFIG_TRACE_BLK_EVENTS
static void block_event_log(unsigned event, const block_interface_t* restrict interface)
{
    char hrsize_buffer[256];

    switch(event) {
        case BLOCK_EVENT_INTERFACE_ADDED:
            pr_verbose("block_event: device added: %s", interface->name);
            break;

        case BLOCK_EVENT_INTERFACE_REMOVED:
            pr_verbose("block_event: device removed: %s", interface->name);
            break;

        case BLOCK_EVENT_MEDIA_CONNECTED:
            format_size_iec(hrsize_buffer, sizeof(hrsize_buffer), interface->sector_count * interface->sector_size);
            pr_verbose("block_event: media connected: %s: %s [%zux%zu]", interface->name, hrsize_buffer, interface->sector_count,
                interface->sector_size);
            break;

        case BLOCK_EVENT_MEDIA_DISCONNECTED:
            pr_verbose("block_event: media disconnected: %s", interface->name);
            break;

        case BLOCK_EVENT_PARTITION_ADDED:
            format_size_iec(hrsize_buffer, sizeof(hrsize_buffer), interface->sector_count * interface->sector_size);
            pr_verbose("block_event: partition added: %s: %s [%zux%zu]", interface->name, hrsize_buffer, interface->sector_count,
                interface->sector_size);
            break;

        case BLOCK_EVENT_PARTITION_REMOVED:
            pr_verbose("block_event: partition removed: %s", interface->name);
            break;
    }
}
#endif

static void replay_device(const block_interface_t* restrict interface, void* restrict arg)
{
    const event_handler_t* entry = arg;

    entry->handler(BLOCK_EVENT_INTERFACE_ADDED, interface, entry->ctxdata);

    if(interface->flags & BLOCK_FLAG_PRESENT) {
        if(interface->flags & BLOCK_FLAG_PARTITION) {
            entry->handler(BLOCK_EVENT_PARTITION_ADDED, interface, entry->ctxdata);
        }
        else {
            entry->handler(BLOCK_EVENT_MEDIA_CONNECTED, interface, entry->ctxdata);
        }
    }
}

void block_event_lock(void)
{
    if(mutex_held(&events_mutex)) {
        events_depth += 1;
        return;
    }

    mutex_acquire(&events_mutex);
}

void block_event_unlock(void)
{
    if(events_depth > 0) {
        events_depth -= 1;
        return;
    }

    mutex_release(&events_mutex);
}

int block_event_subscribe(block_event_handler_t handler, void* restrict ctxdata)
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

    block_interface_for_each(&replay_device, entry);

    mutex_release(&events_mutex);

    return 0;
}

int block_event_unsubscribe(block_event_handler_t handler, void* restrict ctxdata)
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

void block_event_notify(unsigned event, const block_interface_t* restrict interface)
{
#ifdef CONFIG_TRACE_BLK_EVENTS
    block_event_log(event, interface);
#endif

    block_event_lock();

    for(event_handler_t* it = handlers_head; it; it = it->next) {
        it->handler(event, interface, it->ctxdata);
    }

    block_event_unlock();
}

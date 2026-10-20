#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/errno.h>
#include <vex/hrsize.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>

typedef struct event_handler_s event_handler_t;

struct event_handler_s {
    event_handler_t* next;
    blk_event_handler_t handler;
    void* ctxdata;
};

static mutex_t events_mutex = MUTEX_INIT;
static unsigned events_depth = 0;
static event_handler_t* handlers_head = nullptr;

#ifdef CONFIG_TRACE_BLK_EVENTS
static void blk_event_log(unsigned event, const blk_device_t* restrict device)
{
    char hrsize_buffer[256];

    switch(event) {
        case BLK_EVENT_DEVICE_ADDED:
            pr_verbose("blk_event: device added: %s", device->name);
            break;

        case BLK_EVENT_DEVICE_REMOVED:
            pr_verbose("blk_event: device removed: %s", device->name);
            break;

        case BLK_EVENT_MEDIA_CONNECTED:
            format_size_iec(hrsize_buffer, sizeof(hrsize_buffer), device->sector_count * device->sector_size);
            pr_verbose("blk_event: media connected: %s: %s [%zux%zu]", device->name, hrsize_buffer, device->sector_count,
                device->sector_size);
            break;

        case BLK_EVENT_MEDIA_DISCONNECTED:
            pr_verbose("blk_event: media disconnected: %s", device->name);
            break;

        case BLK_EVENT_PARTITION_ADDED:
            format_size_iec(hrsize_buffer, sizeof(hrsize_buffer), device->sector_count * device->sector_size);
            pr_verbose("blk_event: partition added: %s: %s [%zux%zu]", device->name, hrsize_buffer, device->sector_count,
                device->sector_size);
            break;

        case BLK_EVENT_PARTITION_REMOVED:
            pr_verbose("blk_event: partition removed: %s", device->name);
            break;
    }
}
#endif

static void replay_device(const blk_device_t* restrict device, void* restrict arg)
{
    const event_handler_t* entry = arg;

    entry->handler(BLK_EVENT_DEVICE_ADDED, device, entry->ctxdata);

    if(device->flags & BLK_FLAG_PRESENT) {
        if(device->flags & BLK_FLAG_PARTITION) {
            entry->handler(BLK_EVENT_PARTITION_ADDED, device, entry->ctxdata);
        }
        else {
            entry->handler(BLK_EVENT_MEDIA_CONNECTED, device, entry->ctxdata);
        }
    }
}

void blk_event_lock(void)
{
    if(mutex_held(&events_mutex)) {
        events_depth += 1;
        return;
    }

    mutex_acquire(&events_mutex);
}

void blk_event_unlock(void)
{
    if(events_depth > 0) {
        events_depth -= 1;
        return;
    }

    mutex_release(&events_mutex);
}

int blk_event_subscribe(blk_event_handler_t handler, void* restrict ctxdata)
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

    blk_device_for_each(&replay_device, entry);

    mutex_release(&events_mutex);

    return 0;
}

int blk_event_unsubscribe(blk_event_handler_t handler, void* restrict ctxdata)
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

void blk_event_notify(unsigned event, const blk_device_t* restrict device)
{
#ifdef CONFIG_TRACE_BLK_EVENTS
    blk_event_log(event, device);
#endif

    blk_event_lock();

    for(event_handler_t* it = handlers_head; it; it = it->next) {
        it->handler(event, device, it->ctxdata);
    }

    blk_event_unlock();
}

#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/errno.h>
#include <vex/hrsize.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>

struct event_handler {
    struct event_handler* next;
    blk_event_handler_type handler;
    void* ctxdata;
};

static struct mutex events_mutex = MUTEX_INIT;
static unsigned events_depth = 0;
static struct event_handler* handlers_head = nullptr;

#ifdef CONFIG_TRACE_BLK_EVENTS
static void blk_event_log(unsigned event, const struct blk_device* restrict device)
{
    char hrsize_buffer[256];

    switch(event) {
        case BLK_EVENT_DEVICE_ADDED:
            pr_verbose("blk_event: device added: %s", device->identifier);
            break;

        case BLK_EVENT_DEVICE_REMOVED:
            pr_verbose("blk_event: device removed: %s", device->identifier);
            break;

        case BLK_EVENT_MEDIA_CONNECTED:
            hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), device->sector_count * device->sector_size);
            pr_verbose("blk_event: media connected: %s: %s [%zux%zu]", device->identifier, hrsize_buffer, device->sector_count,
                device->sector_size);
            break;

        case BLK_EVENT_MEDIA_DISCONNECTED:
            pr_verbose("blk_event: media disconnected: %s", device->identifier);
            break;

        case BLK_EVENT_PARTITION_ADDED:
            hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), device->sector_count * device->sector_size);
            pr_verbose("blk_event: partition added: %s: %s [%zux%zu]", device->identifier, hrsize_buffer, device->sector_count,
                device->sector_size);
            break;

        case BLK_EVENT_PARTITION_REMOVED:
            pr_verbose("blk_event: partition removed: %s", device->identifier);
            break;
    }
}
#endif

static void replay_device(const struct blk_device* restrict device, void* restrict arg)
{
    const struct event_handler* entry = arg;

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

int blk_event_subscribe(blk_event_handler_type handler, void* restrict ctxdata)
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

    iterate_blk_devices(&replay_device, entry);

    mutex_release(&events_mutex);

    return 0;
}

int blk_event_unsubscribe(blk_event_handler_type handler, void* restrict ctxdata)
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

void blk_event_notify(unsigned event, const struct blk_device* restrict device)
{
#ifdef CONFIG_TRACE_BLK_EVENTS
    blk_event_log(event, device);
#endif

    blk_event_lock();

    for(struct event_handler* it = handlers_head; it; it = it->next) {
        it->handler(event, device, it->ctxdata);
    }

    blk_event_unlock();
}

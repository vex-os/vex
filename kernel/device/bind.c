#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/sched.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/strings.h>

#include "device_internal.h"

extern const driver_t drivers_start[];
extern const driver_t drivers_end[];

typedef struct candidate_s candidate_t;

struct candidate_s {
    const driver_t* driver;
    const driver_match_t* match;
    int score;
};

static spinlock_t queue_lock = SPINLOCK_INIT; // Taken with interrupts disabled
static device_t* queue_head = nullptr;
static device_t* queue_tail = nullptr;
static event_t worker_wakeup;

static bool matches(device_t* restrict device, const driver_match_t* restrict match)
{
    if(match->kind == DRIVER_MATCH_FIRMWARE) {
        const char* const ids[] = {
            match->firmware_id,
            nullptr,
        };

        return device->fwnode.ops && fwnode_matches(&device->fwnode, ids);
    }

    device_t* bus = device_bus_of(device);

    return bus && bus->bus_ops->match && bus->bus_ops->match(bus, device, match);
}

static const driver_match_t* find_match(device_t* restrict device, const driver_t* restrict driver)
{
    for(const driver_match_t* match = driver->matches; match->kind != DRIVER_MATCH_END; match += 1) {
        if(matches(device, match)) {
            return match;
        }
    }

    return nullptr;
}

static bool is_complete(const driver_t* restrict driver)
{
    bool can_come_back = (driver->ops->offline == nullptr) == (driver->ops->online == nullptr);

    return driver->ops->detach && driver->ops->release && can_come_back;
}

static void undo_attach(device_t* restrict device, const driver_t* restrict driver)
{
    device_release_resources(device);
    driver->ops->release(device);
    device->ctxdata = nullptr;
}

static int score(device_t* restrict device, const driver_t* restrict driver, const driver_match_t** restrict match)
{
    *match = nullptr;

    if(is_complete(driver)) {
        *match = find_match(device, driver);
    }

    if(*match == nullptr) {
        return -ENODEV;
    }

    int retval = driver->ops->probe(device, *match);

    kassert(retval);

    return retval;
}

// A driver that fails to attach leaves the device unbound,
// rather than to one that scored lower and may not suit it
static void bind(device_t* restrict device)
{
    candidate_t best = {};

    for(const driver_t* driver = drivers_start; driver < drivers_end; driver += 1) {
        const driver_match_t* match;
        int retval = score(device, driver, &match);

        if(retval <= best.score) {
            continue;
        }

        best.driver = driver;
        best.match = match;
        best.score = retval;
    }

    if(best.driver == nullptr) {
        return;
    }

    char path[DEVICE_PATH_MAX];
    device_get_path(device, path, sizeof(path));

    device->priv.state = DEVICE_PROBING;

    int retval = device_wait_for_dependencies(device);

    if(retval == 0) {
        retval = best.driver->ops->attach(device, best.match);

        if(retval < 0) {
            undo_attach(device, best.driver);
        }
    }

    if(retval < 0) {
        device_unlink_providers(device);
    }

    if(retval == -EAGAIN && device_is_waiting(device)) {
        device_log_wait(device, path, best.driver);
        device_start_waiting(device);
        return;
    }

    if(retval < 0) {
        pr_warning("device: %s: %s failed to attach: %s", path, best.driver->name, strerror(retval));
        device_stop_waiting(device);
        device->priv.state = DEVICE_PRESENT;
        device->priv.attach_failed = true;
        return;
    }

    device->driver = best.driver;
    device->priv.state = DEVICE_BOUND;
    pr_verbose("device: %s bound to %s", path, best.driver->name);

    device_wake_waiters(device);
}

static void rescan_node(device_t* device)
{
    if(device->priv.state == DEVICE_GONE) {
        return;
    }

    if(device->driver == nullptr && device->priv.state != DEVICE_WAITING && !device->priv.attach_failed) {
        bind(device);
    }

    if(device->bus_ops && device->bus_ops->rescan) {
        device->bus_ops->rescan(device);
    }

    if(device->fwnode.ops) {
        device_sync_firmware_children(device);
    }

    device_t* child = device_next_child(device, nullptr);

    while(child) {
        rescan_node(child);

        device_t* next = device_next_child(device, child);
        device_decref(child);
        child = next;
    }
}

static void remove_node(device_t* device);

static void remove_children(device_t* device)
{
    for(device_t* child = device_next_child(device, nullptr); child; child = device_next_child(device, nullptr)) {
        remove_node(child);
        device_decref(child);
    }
}

static void unbind_node(device_t* device);

// They probe again, and wait for the node unless it is
// leaving the tree and the firmware lets them do without
static void unbind_consumers(device_t* device)
{
    for(;;) {
        mutex_acquire(&device_tree_mutex);

        device_link_t* link = device->priv.consumers;
        device_t* consumer = nullptr;

        if(link) {
            consumer = link->consumer;
            device_incref(consumer);
        }

        mutex_release(&device_tree_mutex);

        if(consumer == nullptr) {
            return;
        }

        unbind_node(consumer);
        device_queue_request(consumer, DEVICE_REQUEST_RESCAN);
        device_decref(consumer);
    }
}

static void unbind_node(device_t* device)
{
    device_stop_waiting(device);
    unbind_consumers(device);
    remove_children(device);

    device->priv.offline = false;
    device->priv.attach_failed = false;

    const driver_t* driver = device->driver;

    if(driver == nullptr) {
        return;
    }

    char path[DEVICE_PATH_MAX];
    device_get_path(device, path, sizeof(path));

    device->priv.state = DEVICE_DETACHING;

    driver->ops->detach(device);
    undo_attach(device, driver);
    device_unlink_providers(device);

    device->driver = nullptr;
    device->priv.state = DEVICE_PRESENT;

    pr_verbose("device: %s unbound from %s", path, driver->name);
}

// The tree's reference goes last, so the node stays with whoever
// holds others, such as open interfaces; the root stays, unbound
static void remove_node(device_t* device)
{
    if(device->priv.state == DEVICE_GONE) {
        return;
    }

    unbind_node(device);

    if(device->parent == nullptr) {
        return;
    }

    char path[DEVICE_PATH_MAX];
    device_get_path(device, path, sizeof(path));

    device_t* bus = device_bus_of(device);

    if(bus && bus->bus_ops->remove_child) {
        bus->bus_ops->remove_child(bus, device);
    }

    device_t* parent = device->parent;

    mutex_acquire(&device_tree_mutex);

    device_t** prev = &parent->priv.children;

    while(*prev != device) {
        prev = &(*prev)->priv.next;
    }

    *prev = device->priv.next;
    device->priv.next = nullptr;
    device->priv.state = DEVICE_GONE;

    mutex_release(&device_tree_mutex);

    pr_verbose("device: removed %s", path);

    device_wake_waiters(device);
    device_decref(device);
}

static bool asks_driver(const device_t* restrict device)
{
    return device->driver && device->driver->ops->offline && !device_is_gone(device);
}

// Links change only on device_worker, which
// runs this, so they are walked without the tree's lock
static int offline_node(device_t* device)
{
    if(device->priv.offline) {
        return 0;
    }

    for(device_t* child = device->priv.children; child; child = child->priv.next) {
        int retval = offline_node(child);

        if(retval < 0) {
            return retval;
        }
    }

    for(device_link_t* link = device->priv.consumers; link; link = link->next_of_provider) {
        int retval = offline_node(link->consumer);

        if(retval < 0) {
            return retval;
        }
    }

    if(asks_driver(device)) {
        int retval = device->driver->ops->offline(device);

        if(retval < 0) {
            return retval;
        }
    }

    device->priv.offline = true;

    return 0;
}

static void online_node(device_t* device)
{
    if(!device->priv.offline) {
        return;
    }

    device->priv.offline = false;

    if(asks_driver(device)) {
        device->driver->ops->online(device);
    }

    for(device_t* child = device->priv.children; child; child = child->priv.next) {
        online_node(child);
    }

    for(device_link_t* link = device->priv.consumers; link; link = link->next_of_provider) {
        online_node(link->consumer);
    }
}

// What a node takes with it goes only if all of it agrees
static bool take_offline(device_t* restrict device)
{
    int retval = offline_node(device);

    if(retval == 0) {
        return true;
    }

    online_node(device);

    char path[DEVICE_PATH_MAX];
    device_get_path(device, path, sizeof(path));
    pr_warning("device: %s stays: %s", path, strerror(retval));

    return false;
}

static device_t* queue_pop(unsigned* restrict requests)
{
    bool intr_state = spinlock_acquire_intr(&queue_lock);
    device_t* device = queue_head;

    if(device) {
        queue_head = device->priv.worker_next;

        if(queue_head == nullptr) {
            queue_tail = nullptr;
        }

        *requests = device->priv.pending;
        device->priv.pending = 0;
    }

    spinlock_release_intr(&queue_lock, intr_state);

    return device;
}

// A removal makes the node's other requests moot, and an unbind comes
// before a rescan so that a node asked for both ends up bound anew
static void run_queue(void)
{
    unsigned requests;

    for(device_t* device = queue_pop(&requests); device; device = queue_pop(&requests)) {
        if(requests & DEVICE_REQUEST_REMOVE) {
            if(take_offline(device)) {
                remove_node(device);
            }
        }
        else {
            if((requests & DEVICE_REQUEST_UNBIND) && take_offline(device)) {
                unbind_node(device);
            }

            if(requests & DEVICE_REQUEST_RESCAN) {
                rescan_node(device);
            }
        }

        device_decref(device);
    }
}

// Every change to the tree goes through here, so probe, attach and
// detach may sleep and need no lock against each other
static void __noreturn device_worker(void* restrict arg)
{
    for(;;) {
        event_t* wakeup = &worker_wakeup;
        event_await_blocking(&wakeup, 1);
        run_queue();
    }
}

void device_queue_request(device_t* restrict device, unsigned request)
{
    bool intr_state = spinlock_acquire_intr(&queue_lock);
    bool queued = device->priv.pending;

    device->priv.pending |= request;

    if(!queued) {
        device_incref(device);
        device->priv.worker_next = nullptr;

        if(queue_tail) {
            queue_tail->priv.worker_next = device;
        }
        else {
            queue_head = device;
        }

        queue_tail = device;
    }

    spinlock_release_intr(&queue_lock, intr_state);

    if(!queued) {
        event_trigger(&worker_wakeup, true);
    }
}

int device_add_bound(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name, const driver_t* restrict driver,
    void* restrict ctxdata, device_t** restrict device)
{
    kassert(is_complete(driver));

    int retval = device_add(parent, fwnode, name, device);

    if(retval < 0) {
        return retval;
    }

    device_t* added = *device;
    added->driver = driver;
    added->ctxdata = ctxdata;
    added->priv.state = DEVICE_BOUND;

    char path[DEVICE_PATH_MAX];
    device_get_path(added, path, sizeof(path));
    pr_verbose("device: %s bound to %s", path, driver->name);

    return 0;
}

void device_rescan(device_t* restrict device)
{
    device_queue_request(device, DEVICE_REQUEST_RESCAN);
}

void device_unbind(device_t* restrict device)
{
    device_queue_request(device, DEVICE_REQUEST_UNBIND);
}

void device_remove(device_t* restrict device)
{
    device_queue_request(device, DEVICE_REQUEST_REMOVE);
}

// After the registries that drivers add their functions to, and before the
// boot framebuffers, so a display driver can claim the memory of its own;
// the scheduler doesn't run yet, so the queue is run here and not by the worker
static void init_device_tree(void)
{
    event_init(&worker_wakeup);

    for(const driver_t* driver = drivers_start; driver < drivers_end; driver += 1) {
        if(!is_complete(driver)) {
            pr_warning("device: %s can't let go of a device, or come back from offline, left out", driver->name);
        }
    }

    device_t* root = device_get_root();

    if(root == nullptr) {
        panic("device: out of memory");
        unreachable();
    }

    device_rescan(root);
    run_queue();

    device_decref(root);

    if(!thread_spawn_kernel(&device_worker, nullptr)) {
        panic("device: out of memory");
        unreachable();
    }
}

initcall_bsp_dev(init_device_tree, INIT_LATE);

#include <vex/atomic.h>
#include <vex/device.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mmio.h>
#include <vex/mutex.h>
#include <vex/panic.h>
#include <vex/sched.h>
#include <vex/spinlock.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vprot.h>

extern const driver_t drivers_start[];
extern const driver_t drivers_end[];

#define DEVICE_RESOURCE_MAX 16
#define DEVICE_PATH_MAX     256

#define DEVICE_REQUEST_RESCAN 0x01U
#define DEVICE_REQUEST_UNBIND 0x02U
#define DEVICE_REQUEST_REMOVE 0x04U

typedef struct resource_list_s resource_list_t;
typedef struct candidate_s candidate_t;
typedef struct dependency_walk_s dependency_walk_t;

struct device_irq_s {
    device_irq_t* next;
    unsigned index;
    interrupt_action_t* action;
};

struct device_mapping_s {
    device_mapping_t* next;
    io_region_t region;
};

struct device_link_s {
    device_t* consumer;
    device_t* provider;
    device_link_t* next_of_consumer;
    device_link_t* next_of_provider;
};

struct resource_list_s {
    fwnode_resource_t resources[DEVICE_RESOURCE_MAX];
    size_t num_resources;
};

struct candidate_s {
    const driver_t* driver;
    const driver_match_t* match;
    unsigned bid;
};

struct dependency_walk_s {
    device_t* device;
    int retval;
};

// Firmware lists the power engine plugin (Windows' PEP) in _DEP for
// the OS's power management, which no driver of the tree stands for
static const char* const ignored_providers[] = {
    "PNP0D80",
    nullptr,
};

// Guards the links between nodes
// and the interrupts requested on them
static mutex_t tree_mutex = MUTEX_INIT;
static device_t* root = nullptr;
static device_t* waiting_head = nullptr; // Under the tree's lock

static spinlock_t queue_lock = SPINLOCK_INIT; // Taken with interrupts disabled
static device_t* queue_head = nullptr;
static device_t* queue_tail = nullptr;
static event_t worker_wakeup;

static bool is_range(const fwnode_resource_t* restrict resource)
{
    return resource->type == FWNODE_RESOURCE_PORT || resource->type == FWNODE_RESOURCE_MEMORY;
}

// A window is what a node passes on to its children, not registers of its own
static void collect_resource(const fwnode_resource_t* resource, void* arg)
{
    resource_list_t* list = arg;

    if(is_range(resource) && (resource->range.flags & FWNODE_RANGE_WINDOW)) {
        return;
    }

    if(list->num_resources == DEVICE_RESOURCE_MAX) {
        return;
    }

    list->resources[list->num_resources] = *resource;
    list->num_resources += 1;
}

static int copy_resources(device_t* restrict device)
{
    resource_list_t list = {};

    device->priv.resources = nullptr;
    device->priv.num_resources = 0;

    if(device->fwnode.ops == nullptr) {
        return 0;
    }

    fwnode_for_each_resource(&device->fwnode, &collect_resource, &list);

    if(list.num_resources == 0) {
        return 0;
    }

    device->priv.resources = kmalloc(sizeof(fwnode_resource_t) * list.num_resources);

    if(device->priv.resources == nullptr) {
        return -ENOMEM;
    }

    memcpy(device->priv.resources, list.resources, sizeof(fwnode_resource_t) * list.num_resources);
    device->priv.num_resources = list.num_resources;

    return 0;
}

static device_t* allocate(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name)
{
    device_t* device = kmalloc(sizeof(device_t));

    if(device == nullptr) {
        return nullptr;
    }

    device->fwnode = (fwnode_t) {};

    if(fwnode) {
        device->fwnode = *fwnode;
    }

    if(copy_resources(device) < 0) {
        kfree(device);
        return nullptr;
    }

    device->priv.next = nullptr;
    device->priv.children = nullptr;
    device->priv.irqs = nullptr;
    device->priv.mappings = nullptr;
    device->priv.providers = nullptr;
    device->priv.consumers = nullptr;
    device->priv.awaited = nullptr;
    device->priv.awaited_fwnode = (fwnode_t) {};
    device->priv.waiting_next = nullptr;
    device->priv.worker_next = nullptr;
    device->priv.pending = 0;
    device->priv.state = DEVICE_PRESENT;
    device->priv.fw_child = false;
    device->priv.fw_children_placed = false;
    device->priv.gone = false;
    atomic_store(&device->priv.refcount, 1, ATOMIC_RELAXED);

    device->parent = parent;
    device->bus_ops = nullptr;
    device->bus_ctxdata = nullptr;
    device->driver = nullptr;
    device->ctxdata = nullptr;

    kstrncpy(device->name, name, sizeof(device->name) - 1);
    device->name[sizeof(device->name) - 1] = 0;

    return device;
}

static size_t write_path(const device_t* restrict device, char* restrict buffer, size_t size)
{
    if(device->parent == nullptr) {
        return 0;
    }

    size_t length = write_path(device->parent, buffer, size);
    size_t offset = length < size ? length : size;

    if(length == 0) {
        return length + snprintf(buffer + offset, size - offset, "%s", device->name);
    }
    else {
        return length + snprintf(buffer + offset, size - offset, "/%s", device->name);
    }
}

// The bus a node hangs from, or nullptr when the node only hangs
// from its parent as the firmware describes it, without a bus between
static device_t* bus_of(device_t* restrict device)
{
    device_t* bus = device->parent;

    if(bus == nullptr || bus->bus_ops == nullptr || device->priv.fw_child) {
        return nullptr;
    }

    return bus;
}

static void place_fw_child(const fwnode_t* child, void* arg)
{
    device_t* parent = arg;

    if(!fwnode_is_present(child)) {
        return;
    }

    if(parent->bus_ops && parent->bus_ops->place_child) {
        int retval = parent->bus_ops->place_child(parent, child);

        if(retval != -ENOENT) {
            return;
        }
    }

    char name[DEVICE_NAME_MAX];

    if(fwnode_get_name(child, name, sizeof(name)) < 0) {
        return;
    }

    device_t* device = nullptr;

    if(device_add(parent, child, name, &device) < 0) {
        pr_warning("device: out of memory");
        return;
    }

    device->priv.fw_child = true;
    device_decref(device);
}

static bool matches(device_t* restrict device, const driver_match_t* restrict match)
{
    if(match->kind == DRIVER_MATCH_FW) {
        const char* const ids[] = {
            match->fw_id,
            nullptr,
        };

        return device->fwnode.ops && fwnode_matches(&device->fwnode, ids);
    }

    device_t* bus = bus_of(device);

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
    return driver->ops->detach && driver->ops->release;
}

static void release_resources(device_t* restrict device)
{
    device_t* bus = bus_of(device);

    if(bus && bus->bus_ops->release_child) {
        bus->bus_ops->release_child(bus, device);
    }

    mutex_acquire(&tree_mutex);
    device_irq_t* irqs = device->priv.irqs;
    device_mapping_t* mappings = device->priv.mappings;
    device->priv.irqs = nullptr;
    device->priv.mappings = nullptr;
    mutex_release(&tree_mutex);

    while(irqs) {
        device_irq_t* next = irqs->next;
        interrupt_detach(irqs->action);
        kfree(irqs);
        irqs = next;
    }

    while(mappings) {
        device_mapping_t* next = mappings->next;
        device_unmap_range(&mappings->region);
        kfree(mappings);
        mappings = next;
    }
}

static void queue_request(device_t* restrict device, unsigned request)
{
    bool intr_state = spinlock_acquire_intr(&queue_lock);
    bool queued = device->priv.pending != 0;

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

static bool is_waiting(const device_t* restrict device)
{
    return device->priv.awaited || device->priv.awaited_fwnode.ops;
}

// A node relies on its parent, on the providers its driver uses and on the one it waits for
static bool relies_on_nolock(const device_t* device, const device_t* provider)
{
    if(device == provider) {
        return true;
    }

    if(device->parent && relies_on_nolock(device->parent, provider)) {
        return true;
    }

    for(const device_link_t* link = device->priv.providers; link; link = link->next_of_consumer) {
        if(relies_on_nolock(link->provider, provider)) {
            return true;
        }
    }

    return device->priv.awaited && relies_on_nolock(device->priv.awaited, provider);
}

static device_t* find_by_fwnode_nolock(device_t* parent, const fwnode_t* restrict fwnode)
{
    for(device_t* child = parent->priv.children; child; child = child->priv.next) {
        if(child->fwnode.ops && fwnode_is_same(&child->fwnode, fwnode)) {
            return child;
        }

        device_t* found = find_by_fwnode_nolock(child, fwnode);

        if(found) {
            return found;
        }
    }

    return nullptr;
}

static device_t* find_by_fwnode(const fwnode_t* restrict fwnode)
{
    mutex_acquire(&tree_mutex);

    device_t* device = find_by_fwnode_nolock(root, fwnode);

    if(device) {
        device_incref(device);
    }

    mutex_release(&tree_mutex);

    return device;
}

// A provider the firmware says isn't there is one the node does without,
// as the firmware would then have it work without it
static int wait_for_fwnode(device_t* restrict device, const fwnode_t* restrict fwnode)
{
    if(fwnode_matches(fwnode, ignored_providers) || !fwnode_is_present(fwnode)) {
        return 0;
    }

    device_t* provider = find_by_fwnode(fwnode);

    if(provider == nullptr) {
        device->priv.awaited_fwnode = *fwnode;
        return -EAGAIN;
    }

    int retval = device_wait_for(device, provider);

    device_decref(provider);

    return retval;
}

static void wait_for_dependency(const fwnode_t* provider, void* arg)
{
    dependency_walk_t* walk = arg;

    if(walk->retval == 0) {
        walk->retval = wait_for_fwnode(walk->device, provider);
    }
}

static int wait_for_dependencies(device_t* restrict device)
{
    dependency_walk_t walk = {};
    walk.device = device;

    if(device->fwnode.ops) {
        fwnode_for_each_dependency(&device->fwnode, &wait_for_dependency, &walk);
    }

    return walk.retval;
}

static void unlink_providers(device_t* restrict device)
{
    mutex_acquire(&tree_mutex);

    device_link_t* links = device->priv.providers;
    device->priv.providers = nullptr;

    for(device_link_t* link = links; link; link = link->next_of_consumer) {
        device_link_t** prev = &link->provider->priv.consumers;

        while(*prev != link) {
            prev = &(*prev)->next_of_provider;
        }

        *prev = link->next_of_provider;
    }

    mutex_release(&tree_mutex);

    while(links) {
        device_link_t* next = links->next_of_consumer;
        device_decref(links->provider);
        kfree(links);
        links = next;
    }
}

static void start_waiting(device_t* restrict device)
{
    mutex_acquire(&tree_mutex);
    device->priv.waiting_next = waiting_head;
    waiting_head = device;
    device->priv.state = DEVICE_WAITING;
    mutex_release(&tree_mutex);
}

static void stop_waiting(device_t* restrict device)
{
    mutex_acquire(&tree_mutex);

    device_t* awaited = device->priv.awaited;
    device->priv.awaited = nullptr;
    device->priv.awaited_fwnode = (fwnode_t) {};

    if(device->priv.state == DEVICE_WAITING) {
        device_t** prev = &waiting_head;

        while(*prev != device) {
            prev = &(*prev)->priv.waiting_next;
        }

        *prev = device->priv.waiting_next;
        device->priv.waiting_next = nullptr;
        device->priv.state = DEVICE_PRESENT;
    }

    mutex_release(&tree_mutex);

    if(awaited) {
        device_decref(awaited);
    }
}

static bool awaits_nolock(const device_t* restrict waiter, const device_t* restrict provider)
{
    if(waiter->priv.awaited == provider) {
        return true;
    }

    return waiter->priv.awaited_fwnode.ops && provider->fwnode.ops && fwnode_is_same(&waiter->priv.awaited_fwnode, &provider->fwnode);
}

// When the provider is bound or leaves the tree, so its waiters
// either attach or find out what they wait for won't come
static void wake_waiters(device_t* restrict provider)
{
    mutex_acquire(&tree_mutex);

    device_t** prev = &waiting_head;

    while(*prev) {
        device_t* waiter = *prev;

        if(!awaits_nolock(waiter, provider)) {
            prev = &waiter->priv.waiting_next;
            continue;
        }

        *prev = waiter->priv.waiting_next;
        waiter->priv.waiting_next = nullptr;
        waiter->priv.awaited_fwnode = (fwnode_t) {};
        waiter->priv.state = DEVICE_PRESENT;

        if(waiter->priv.awaited) {
            waiter->priv.awaited = nullptr;
            device_decref(provider);
        }

        queue_request(waiter, DEVICE_REQUEST_RESCAN);
    }

    mutex_release(&tree_mutex);
}

static void undo_attach(device_t* restrict device, const driver_t* restrict driver)
{
    release_resources(device);
    driver->ops->release(device);
    device->ctxdata = nullptr;
}

static void log_wait(device_t* restrict device, const char* restrict path, const driver_t* restrict driver)
{
    if(device->priv.awaited) {
        char provider_path[DEVICE_PATH_MAX];
        device_get_path(device->priv.awaited, provider_path, sizeof(provider_path));
        pr_verbose("device: %s: %s waits for %s", path, driver->name, provider_path);
    }
    else {
        char provider_name[DEVICE_NAME_MAX];

        if(fwnode_get_name(&device->priv.awaited_fwnode, provider_name, sizeof(provider_name)) < 0) {
            provider_name[0] = 0;
        }

        pr_verbose("device: %s: %s waits for %s, not in the tree yet", path, driver->name, provider_name);
    }
}

// A driver that fails to attach leaves the device unbound,
// rather than to one that bid lower and may not suit it
static void bind(device_t* restrict device)
{
    candidate_t best = {};

    for(const driver_t* driver = drivers_start; driver < drivers_end; driver += 1) {
        const driver_match_t* match = nullptr;
        unsigned bid = 0;

        if(is_complete(driver)) {
            match = find_match(device, driver);
        }

        if(match == nullptr || driver->ops->probe(device, match, &bid) < 0 || bid <= best.bid) {
            continue;
        }

        best.driver = driver;
        best.match = match;
        best.bid = bid;
    }

    if(best.driver == nullptr) {
        return;
    }

    char path[DEVICE_PATH_MAX];
    device_get_path(device, path, sizeof(path));

    device->priv.state = DEVICE_PROBING;

    int retval = wait_for_dependencies(device);

    if(retval == 0) {
        retval = best.driver->ops->attach(device, best.match);

        if(retval < 0) {
            undo_attach(device, best.driver);
        }
    }

    if(retval < 0) {
        unlink_providers(device);
    }

    if(retval == -EAGAIN && is_waiting(device)) {
        log_wait(device, path, best.driver);
        start_waiting(device);
        return;
    }

    if(retval < 0) {
        pr_warning("device: %s: %s failed to attach: %s", path, best.driver->name, strerror(retval));
        stop_waiting(device);
        device->priv.state = DEVICE_PRESENT;
        return;
    }

    device->driver = best.driver;
    device->priv.state = DEVICE_BOUND;
    pr_verbose("device: %s bound to %s", path, best.driver->name);

    wake_waiters(device);
}

// The caller's reference to the previous child keeps it in the list; the
// walks over the tree take no restrict, as removing a child changes its parent
static device_t* next_child(device_t* parent, device_t* prev)
{
    mutex_acquire(&tree_mutex);

    device_t* child = parent->priv.children;

    if(prev) {
        child = prev->priv.next;
    }

    if(child) {
        device_incref(child);
    }

    mutex_release(&tree_mutex);

    return child;
}

static void rescan_node(device_t* device)
{
    if(device->priv.state == DEVICE_GONE) {
        return;
    }

    if(device->driver == nullptr && device->priv.state != DEVICE_WAITING) {
        bind(device);
    }

    if(!device->priv.fw_children_placed && device->fwnode.ops) {
        fwnode_for_each_child(&device->fwnode, &place_fw_child, device);
    }

    device->priv.fw_children_placed = true;

    device_t* child = next_child(device, nullptr);

    while(child) {
        rescan_node(child);

        device_t* next = next_child(device, child);
        device_decref(child);
        child = next;
    }
}

static void remove_node(device_t* device);

static void remove_children(device_t* device)
{
    for(device_t* child = next_child(device, nullptr); child; child = next_child(device, nullptr)) {
        remove_node(child);
        device_decref(child);
    }

    device->priv.fw_children_placed = false;
}

static void unbind_node(device_t* device);

// They probe again, and wait for the node unless it is
// leaving the tree and the firmware lets them do without
static void unbind_consumers(device_t* device)
{
    for(;;) {
        mutex_acquire(&tree_mutex);

        device_link_t* link = device->priv.consumers;
        device_t* consumer = nullptr;

        if(link) {
            consumer = link->consumer;
            device_incref(consumer);
        }

        mutex_release(&tree_mutex);

        if(consumer == nullptr) {
            return;
        }

        unbind_node(consumer);
        queue_request(consumer, DEVICE_REQUEST_RESCAN);
        device_decref(consumer);
    }
}

static void unbind_node(device_t* device)
{
    stop_waiting(device);
    unbind_consumers(device);
    remove_children(device);

    const driver_t* driver = device->driver;

    if(driver == nullptr) {
        return;
    }

    char path[DEVICE_PATH_MAX];
    device_get_path(device, path, sizeof(path));

    device->priv.state = DEVICE_DETACHING;

    driver->ops->detach(device);
    undo_attach(device, driver);
    unlink_providers(device);

    device->driver = nullptr;
    device->priv.state = DEVICE_PRESENT;

    pr_verbose("device: %s unbound from %s", path, driver->name);
}

// The tree's reference goes last, so the node stays with whoever
// holds others, such as open functions; the root stays, unbound
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

    device_t* bus = bus_of(device);

    if(bus && bus->bus_ops->remove_child) {
        bus->bus_ops->remove_child(bus, device);
    }

    device_t* parent = device->parent;

    mutex_acquire(&tree_mutex);

    device_t** prev = &parent->priv.children;

    while(*prev != device) {
        prev = &(*prev)->priv.next;
    }

    *prev = device->priv.next;
    device->priv.next = nullptr;
    device->priv.state = DEVICE_GONE;

    mutex_release(&tree_mutex);

    pr_verbose("device: removed %s", path);

    wake_waiters(device);
    device_decref(device);
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
            remove_node(device);
        }
        else {
            if(requests & DEVICE_REQUEST_UNBIND) {
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

static void set_gone(device_t* device)
{
    device->priv.gone = true;

    for(device_t* child = device->priv.children; child; child = child->priv.next) {
        set_gone(child);
    }
}

static int fw_get_resource(device_t* restrict device, bool want_irq, unsigned index, fwnode_resource_t* restrict resource)
{
    unsigned found = 0;

    for(size_t i = 0; i < device->priv.num_resources; ++i) {
        const fwnode_resource_t* candidate = &device->priv.resources[i];
        bool is_irq = candidate->type == FWNODE_RESOURCE_IRQ;
        bool wanted = want_irq ? is_irq : is_range(candidate);

        if(!wanted) {
            continue;
        }

        if(found == index) {
            *resource = *candidate;
            return 0;
        }

        found += 1;
    }

    return -ENODEV;
}

static int get_range(device_t* restrict device, unsigned index, fwnode_resource_t* restrict range)
{
    device_t* bus = bus_of(device);

    if(bus && bus->bus_ops->get_range) {
        return bus->bus_ops->get_range(bus, device, index, range);
    }

    return fw_get_resource(device, false, index, range);
}

static int get_irq(device_t* restrict device, unsigned index, fwnode_irq_t* restrict irq)
{
    device_t* bus = bus_of(device);

    if(bus && bus->bus_ops->get_irq) {
        return bus->bus_ops->get_irq(bus, device, index, irq);
    }

    fwnode_resource_t resource;
    int retval = fw_get_resource(device, true, index, &resource);

    if(retval < 0) {
        return retval;
    }

    *irq = resource.irq;

    return 0;
}

static device_irq_t** find_irq_nolock(device_t* restrict device, unsigned index)
{
    device_irq_t** prev = &device->priv.irqs;

    while(*prev && (*prev)->index != index) {
        prev = &(*prev)->next;
    }

    return prev;
}

static int attach_irq(const fwnode_irq_t* restrict irq, irq_handler_t handler, void* restrict ctxdata, interrupt_action_t** restrict action)
{
    uint32_t gsi = irq->number;
    unsigned trigger = irq->trigger;
    unsigned polarity = irq->polarity;
    unsigned flags = 0;

    if(irq->flags & FWNODE_IRQ_ISA) {
        interrupt_isa_route(irq->number, &gsi, &trigger, &polarity);
    }

    if(irq->flags & FWNODE_IRQ_SHARED) {
        flags |= INTERRUPT_SHARED;
    }

    return interrupt_attach(gsi, trigger, polarity, flags, handler, ctxdata, action);
}

device_t* device_get_root(void)
{
    mutex_acquire(&tree_mutex);

    if(root == nullptr) {
        fwnode_t fwnode;
        bool has_fwnode = 0 == fwnode_root(&fwnode);

        root = allocate(nullptr, has_fwnode ? &fwnode : nullptr, "");
    }

    if(root) {
        device_incref(root);
    }

    mutex_release(&tree_mutex);

    return root;
}

int device_add(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name, device_t** restrict device)
{
    device_t* child = allocate(parent, fwnode, name);

    if(child == nullptr) {
        return -ENOMEM;
    }

    device_incref(parent);
    device_incref(child);

    mutex_acquire(&tree_mutex);

    device_t** tail = &parent->priv.children;

    while(*tail) {
        tail = &(*tail)->priv.next;
    }

    *tail = child;

    mutex_release(&tree_mutex);

    char path[DEVICE_PATH_MAX];
    device_get_path(child, path, sizeof(path));
    pr_verbose("device: added %s", path);

    *device = child;

    return 0;
}

void device_rescan(device_t* restrict device)
{
    queue_request(device, DEVICE_REQUEST_RESCAN);
}

void device_unbind(device_t* restrict device)
{
    queue_request(device, DEVICE_REQUEST_UNBIND);
}

void device_remove(device_t* restrict device)
{
    queue_request(device, DEVICE_REQUEST_REMOVE);
}

int device_wait_for(device_t* restrict device, device_t* restrict provider)
{
    kassert(device->priv.state == DEVICE_PROBING);
    kassert(!is_waiting(device));

    device_link_t* link = kmalloc(sizeof(device_link_t));

    if(link == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&tree_mutex);

    int retval = 0;

    if(relies_on_nolock(provider, device)) {
        retval = -EDEADLK;
    }
    else if(provider->priv.state == DEVICE_GONE) {
        retval = -ENODEV;
    }
    else if(provider->priv.state != DEVICE_BOUND) {
        device_incref(provider);
        device->priv.awaited = provider;
        retval = -EAGAIN;
    }
    else {
        device_incref(provider);
        link->consumer = device;
        link->provider = provider;
        link->next_of_consumer = device->priv.providers;
        link->next_of_provider = provider->priv.consumers;
        device->priv.providers = link;
        provider->priv.consumers = link;
        link = nullptr;
    }

    mutex_release(&tree_mutex);

    kfree(link);

    return retval;
}

void device_set_gone(device_t* restrict device)
{
    mutex_acquire(&tree_mutex);
    set_gone(device);
    mutex_release(&tree_mutex);
}

bool device_is_gone(const device_t* restrict device)
{
    return device->priv.gone || device->priv.state == DEVICE_GONE;
}

void device_incref(device_t* restrict device)
{
    atomic_fetch_add(&device->priv.refcount, 1, ATOMIC_RELAXED);
}

void device_decref(device_t* restrict device)
{
    if(1 != atomic_fetch_sub(&device->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        return;
    }

    kassert(device->priv.children == nullptr);
    kassert(device->priv.irqs == nullptr);
    kassert(device->priv.mappings == nullptr);
    kassert(device->priv.providers == nullptr);
    kassert(device->priv.consumers == nullptr);
    kassert(device->priv.awaited == nullptr);

    device_t* parent = device->parent;

    kfree(device->priv.resources);
    kfree(device);

    if(parent) {
        device_decref(parent);
    }
}

size_t device_get_path(const device_t* restrict device, char* restrict buffer, size_t size)
{
    if(size) {
        buffer[0] = 0;
    }

    return write_path(device, buffer, size);
}

int device_translate_range(device_t* restrict bus, unsigned* restrict type, uint64_t* restrict address, uint64_t length)
{
    for(device_t* node = bus; node; node = node->parent) {
        if(node->bus_ops == nullptr || node->bus_ops->translate_range == nullptr) {
            continue;
        }

        int retval = node->bus_ops->translate_range(node, type, address, length);

        if(retval < 0) {
            return retval;
        }
    }

    return 0;
}

int device_map_range(device_t* restrict bus, unsigned type, uint64_t start, uint64_t length, io_region_t* restrict region)
{
    uint64_t address = start;
    int retval = device_translate_range(bus, &type, &address, length);

    if(retval < 0) {
        return retval;
    }

    if(type == FWNODE_RESOURCE_PORT) {
        region->space = IO_SPACE_PORT;
        region->base = address;
        region->length = length;
        return 0;
    }

    void* virt = mmio_map(address, length, VPROT_MMIO);

    if(virt == nullptr) {
        return -ENOMEM;
    }

    region->space = IO_SPACE_MEMORY;
    region->base = (uintptr_t)(virt);
    region->length = length;

    return 0;
}

void device_unmap_range(io_region_t* restrict region)
{
    if(region->space == IO_SPACE_MEMORY) {
        mmio_unmap((volatile void*)(region->base));
    }
}

int device_map_io(device_t* restrict device, unsigned index, io_region_t* restrict region)
{
    fwnode_resource_t range;
    int retval = get_range(device, index, &range);

    if(retval < 0) {
        return retval;
    }

    device_mapping_t* mapping = kmalloc(sizeof(device_mapping_t));

    if(mapping == nullptr) {
        return -ENOMEM;
    }

    retval = device_map_range(device->parent, range.type, range.range.start, range.range.length, &mapping->region);

    if(retval < 0) {
        kfree(mapping);
        return retval;
    }

    *region = mapping->region;

    mutex_acquire(&tree_mutex);
    mapping->next = device->priv.mappings;
    device->priv.mappings = mapping;
    mutex_release(&tree_mutex);

    return 0;
}

void device_unmap_io(device_t* restrict device, io_region_t* restrict region)
{
    mutex_acquire(&tree_mutex);

    device_mapping_t** prev = &device->priv.mappings;

    while(*prev && ((*prev)->region.space != region->space || (*prev)->region.base != region->base)) {
        prev = &(*prev)->next;
    }

    device_mapping_t* mapping = *prev;

    if(mapping) {
        *prev = mapping->next;
    }

    mutex_release(&tree_mutex);

    if(mapping) {
        device_unmap_range(&mapping->region);
        kfree(mapping);
    }
}

int device_request_irq(device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata)
{
    fwnode_irq_t irq;
    int retval = get_irq(device, index, &irq);

    if(retval < 0) {
        return retval;
    }

    device_irq_t* entry = kmalloc(sizeof(device_irq_t));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&tree_mutex);

    if(*find_irq_nolock(device, index)) {
        mutex_release(&tree_mutex);
        kfree(entry);
        return -EBUSY;
    }

    retval = attach_irq(&irq, handler, ctxdata, &entry->action);

    if(retval < 0) {
        mutex_release(&tree_mutex);
        kfree(entry);
        return retval;
    }

    entry->index = index;
    entry->next = device->priv.irqs;
    device->priv.irqs = entry;

    mutex_release(&tree_mutex);

    return 0;
}

void device_release_irq(device_t* restrict device, unsigned index)
{
    mutex_acquire(&tree_mutex);

    device_irq_t** prev = find_irq_nolock(device, index);
    device_irq_t* entry = *prev;

    if(entry) {
        *prev = entry->next;
    }

    mutex_release(&tree_mutex);

    if(entry) {
        interrupt_detach(entry->action);
        kfree(entry);
    }
}

// After the registries that drivers add their functions to, and before the
// boot framebuffers, so a display driver can claim the memory of its own;
// the scheduler doesn't run yet, so the queue is run here and not by the worker
static void init_device_tree(void)
{
    event_init(&worker_wakeup);

    for(const driver_t* driver = drivers_start; driver < drivers_end; driver += 1) {
        if(!is_complete(driver)) {
            pr_warning("device: %s can't let go of a device, left out", driver->name);
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

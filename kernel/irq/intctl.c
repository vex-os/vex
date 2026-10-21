#include <arch/irq.h>
#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/intctl.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/mutex.h>
#include <vex/spinlock.h>

typedef struct interrupt_line_s interrupt_line_t;

// A GSI routed to one vector, with the handlers that share it
struct interrupt_line_s {
    interrupt_line_t* next;
    interrupt_controller_t* controller;
    uint32_t gsi;
    unsigned trigger;
    unsigned polarity;
    unsigned flags;
    unsigned irq_number;
    spinlock_t lock; // Over actions, held while they run
    interrupt_action_t* actions;
};

struct interrupt_action_s {
    interrupt_action_t* next;
    interrupt_line_t* line;
    irq_handler_t handler;
    void* ctxdata;
};

static spinlock_t interrupt_controller_lock = SPINLOCK_INIT;
static interrupt_controller_t* interrupt_controller_head = nullptr;
static mutex_t line_mutex = MUTEX_INIT;
static interrupt_line_t* line_head = nullptr;

static void dispatch_line(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata)
{
    interrupt_line_t* line = ctxdata;

    spinlock_acquire(&line->lock);

    for(interrupt_action_t* action = line->actions; action; action = action->next) {
        action->handler(number, ctx, action->ctxdata);
    }

    spinlock_release(&line->lock);
}

static interrupt_line_t* find_line(uint32_t gsi)
{
    for(interrupt_line_t* line = line_head; line; line = line->next) {
        if(line->gsi == gsi) {
            return line;
        }
    }

    return nullptr;
}

static bool may_join(const interrupt_line_t* restrict line, unsigned trigger, unsigned polarity, unsigned flags)
{
    return (line->flags & flags & INTERRUPT_SHARED) && line->trigger == trigger && line->polarity == polarity;
}

static int create_line(uint32_t gsi, unsigned trigger, unsigned polarity, unsigned flags, interrupt_line_t** restrict created)
{
    interrupt_controller_t* controller = interrupt_controller_find(gsi);

    if(controller == nullptr) {
        return -ENODEV;
    }

    interrupt_line_t* line = kmalloc(sizeof(interrupt_line_t));

    if(line == nullptr) {
        return -ENOMEM;
    }

    line->controller = controller;
    line->gsi = gsi;
    line->trigger = trigger;
    line->polarity = polarity;
    line->flags = flags;
    line->lock = SPINLOCK_INIT;
    line->actions = nullptr;
    line->irq_number = irq_handler_add(UINT_MAX, &dispatch_line, line);

    if(line->irq_number == UINT_MAX) {
        kfree(line);
        return -ENOMEM;
    }

    interrupt_controller_route(controller, gsi, line->irq_number + ARCH_IRQ_BASE, 0, trigger, polarity);

    line->next = line_head;
    line_head = line;

    *created = line;

    return 0;
}

static void destroy_line(interrupt_line_t* restrict line)
{
    interrupt_controller_mask(line->controller, line->gsi);
    irq_handler_remove(line->irq_number);

    interrupt_line_t** prev = &line_head;

    while(*prev != line) {
        prev = &(*prev)->next;
    }

    *prev = line->next;

    kfree(line);
}

int interrupt_controller_add(interrupt_controller_t* restrict controller)
{
    spinlock_acquire(&interrupt_controller_lock);

    for(interrupt_controller_t* it = interrupt_controller_head; it; it = it->priv.next) {
        if(it == controller) {
            spinlock_release(&interrupt_controller_lock);
            return -EEXIST;
        }
    }

    controller->priv.next = interrupt_controller_head;
    interrupt_controller_head = controller;

    spinlock_release(&interrupt_controller_lock);

    size_t gsi_base = controller->gsi_base;
    size_t gsi_count = controller->gsi_count;
    pr_verbose("intctl: adding %s: gsi_base=%zu, gsi_count=%zu", controller->name, gsi_base, gsi_count);

    return 0;
}

interrupt_controller_t* interrupt_controller_find(uint32_t gsi)
{
    spinlock_acquire(&interrupt_controller_lock);

    for(interrupt_controller_t* it = interrupt_controller_head; it; it = it->priv.next) {
        if(gsi >= it->gsi_base && gsi < (it->gsi_base + it->gsi_count)) {
            spinlock_release(&interrupt_controller_lock);
            return it;
        }
    }

    spinlock_release(&interrupt_controller_lock);

    return nullptr;
}

void interrupt_controller_mask(interrupt_controller_t* restrict controller, uint32_t gsi)
{
    controller->ops->mask(controller, gsi);
}

void interrupt_controller_unmask(interrupt_controller_t* restrict controller, uint32_t gsi)
{
    controller->ops->unmask(controller, gsi);
}

void interrupt_controller_route(interrupt_controller_t* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger,
    unsigned polarity)
{
    controller->ops->route(controller, gsi, vector, hart, trigger, polarity);
}

int interrupt_attach(uint32_t gsi, unsigned trigger, unsigned polarity, unsigned flags, irq_handler_t handler, void* restrict ctxdata,
    interrupt_action_t** restrict action)
{
    interrupt_action_t* attached = kmalloc(sizeof(interrupt_action_t));

    if(attached == nullptr) {
        return -ENOMEM;
    }

    attached->handler = handler;
    attached->ctxdata = ctxdata;

    mutex_acquire(&line_mutex);

    interrupt_line_t* line = find_line(gsi);

    if(line && !may_join(line, trigger, polarity, flags)) {
        mutex_release(&line_mutex);
        kfree(attached);
        return -EBUSY;
    }

    int retval = 0;

    if(line == nullptr) {
        retval = create_line(gsi, trigger, polarity, flags, &line);
    }

    if(retval < 0) {
        mutex_release(&line_mutex);
        kfree(attached);
        return retval;
    }

    attached->line = line;

    bool intr_state = spinlock_acquire_intr(&line->lock);
    attached->next = line->actions;
    line->actions = attached;
    spinlock_release_intr(&line->lock, intr_state);

    interrupt_controller_unmask(line->controller, gsi);

    mutex_release(&line_mutex);

    *action = attached;

    return 0;
}

void interrupt_detach(interrupt_action_t* restrict action)
{
    interrupt_line_t* line = action->line;

    mutex_acquire(&line_mutex);

    bool intr_state = spinlock_acquire_intr(&line->lock);

    interrupt_action_t** prev = &line->actions;

    while(*prev != action) {
        prev = &(*prev)->next;
    }

    *prev = action->next;

    spinlock_release_intr(&line->lock, intr_state);

    if(line->actions == nullptr) {
        destroy_line(line);
    }

    mutex_release(&line_mutex);

    kfree(action);
}

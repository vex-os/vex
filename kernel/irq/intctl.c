#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/intctl.h>
#include <vex/kprintf.h>
#include <vex/spinlock.h>

static spinlock_t interrupt_controller_lock = SPINLOCK_INIT;
static interrupt_controller_t* interrupt_controller_head = nullptr;

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

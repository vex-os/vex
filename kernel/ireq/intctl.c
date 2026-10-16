#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/intctl.h>
#include <vex/kprintf.h>
#include <vex/spinlock.h>

static spinlock_type intctl_lock = SPINLOCK_INIT;
static struct intctl* intctl_head = nullptr;

int add_intctl(struct intctl* restrict controller)
{
    spinlock_acquire(&intctl_lock);

    for(struct intctl* it = intctl_head; it; it = it->priv.next) {
        if(it == controller) {
            spinlock_release(&intctl_lock);
            return -EEXIST;
        }
    }

    controller->priv.next = intctl_head;
    intctl_head = controller;

    spinlock_release(&intctl_lock);

    size_t gsi_base = controller->gsi_base;
    size_t gsi_count = controller->gsi_count;
    pr_verbose("intctl: adding %s: gsi_base=%zu, gsi_count=%zu", controller->identifier, gsi_base, gsi_count);

    return 0;
}

struct intctl* find_intctl(uint32_t gsi)
{
    spinlock_acquire(&intctl_lock);

    for(struct intctl* it = intctl_head; it; it = it->priv.next) {
        if(gsi >= it->gsi_base && gsi < (it->gsi_base + it->gsi_count)) {
            spinlock_release(&intctl_lock);
            return it;
        }
    }

    spinlock_release(&intctl_lock);

    return nullptr;
}

void intctl_mask(struct intctl* restrict controller, uint32_t gsi)
{
    controller->ops->mask(controller, gsi);
}

void intctl_unmask(struct intctl* restrict controller, uint32_t gsi)
{
    controller->ops->unmask(controller, gsi);
}

void intctl_route(struct intctl* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger, unsigned polarity)
{
    controller->ops->route(controller, gsi, vector, hart, trigger, polarity);
}

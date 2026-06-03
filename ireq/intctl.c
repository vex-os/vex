#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <ireq/intctl.h>

static struct intctl* intctl_head = nullptr;

int intctl_register(struct intctl* restrict controller)
{
    size_t gsi_base;
    size_t gsi_count;
    struct intctl* it;

    for(it = intctl_head; it; it = it->priv.next) {
        if(it == controller) {
            return -EEXIST;
        }
    }

    gsi_base = controller->gsi_base;
    gsi_count = controller->gsi_count;
    pr_debug("intctl: adding %s: gsi_base=%zu, gsi_count=%zu", controller->identifier, gsi_base, gsi_count);

    controller->priv.next = intctl_head;
    intctl_head = controller;

    return 0;
}

struct intctl* find_intctl(uint32_t gsi)
{
    struct intctl* it;

    for(it = intctl_head; it; it = it->priv.next) {
        if(gsi >= it->gsi_base && gsi < (it->gsi_base + it->gsi_count)) {
            return it;
        }
    }

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

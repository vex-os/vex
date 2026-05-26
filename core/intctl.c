// SPDX-License-Identifier: BSD-3-Clause

#include <core/assert.h>
#include <core/errno.h>
#include <core/intctl.h>
#include <core/kprintf.h>

static struct intctl* intctl_head = nullptr;

int register_intctl(struct intctl* restrict controller)
{
    struct intctl* it;

    kassert(controller && "invalid interrupt controller pointer");
    kassert(controller->ops && "invalid interrupt controller ops pointer");

    for(it = intctl_head; it; it = it->next) {
        if(it == controller)
            return EEXIST;
        continue;
    }

    kprintf(KP_INFORM, "intctl: adding %s: gsi_base=%zu", controller->identity, (size_t)(controller->gsi_base));

    controller->next = intctl_head;
    intctl_head = controller;

    return 0;
}

struct intctl* find_intctl(uint32_t gsi)
{
    struct intctl* it;

    for(it = intctl_head; it; it = it->next) {
        if(gsi >= it->gsi_base && gsi < (it->gsi_base + it->gsi_count)) {
            return it;
        }
    }

    return nullptr;
}

void intctl_mask(struct intctl* restrict controller, uint32_t gsi)
{
    kassert(controller && "invalid interrupt controller pointer");

    controller->ops->mask(controller, gsi);
}

void intctl_unmask(struct intctl* restrict controller, uint32_t gsi)
{
    kassert(controller && "invalid interrupt controller pointer");

    controller->ops->unmask(controller, gsi);
}

void intctl_route(struct intctl* restrict controller, uint32_t gsi, uint8_t vector, uint32_t cpu_id, bool level, bool active_low)
{
    kassert(controller && "invalid interrupt controller pointer");

    controller->ops->route(controller, gsi, vector, cpu_id, level, active_low);
}

// SPDX-License-Identifier: BSD-3-Clause

#include <arch/ireq.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/ireq.h>
#include <core/kprintf.h>
#include <limits.h>

#define IREQ_TABLE_SIZE (IREQ_VECTOR_MAX - IREQ_VECTOR_MIN + 1)

struct ireq_entry {
    ireq_handler_type handler;
    void* privdata;
};

static struct ireq_entry ireq_table[IREQ_TABLE_SIZE] = { 0 };

unsigned int register_ireq_handler(unsigned int desired_vector, ireq_handler_type handler, void* restrict privdata)
{
    unsigned int vector;

    if(desired_vector < IREQ_TABLE_SIZE && ireq_table[desired_vector].handler == nullptr) {
        ireq_table[desired_vector].handler = handler;
        ireq_table[desired_vector].privdata = privdata;
        return desired_vector;
    }

    for(vector = 0; vector < IREQ_TABLE_SIZE; vector += 1) {
        if(ireq_table[vector].handler == nullptr) {
            ireq_table[vector].handler = handler;
            ireq_table[vector].privdata = privdata;
            return vector;
        }
    }

    return UINT_MAX;
}

void unregister_ireq_handler(unsigned int vector)
{
    kassert(vector < IREQ_TABLE_SIZE && "invalid ireq vector");

    ireq_table[vector].handler = nullptr;
    ireq_table[vector].privdata = nullptr;
}

void dispatch_ireq(unsigned int vector, struct interrupt_frame* restrict frame)
{
    ireq_handler_type handler;

    if(vector >= IREQ_TABLE_SIZE) {
        kprintf(KP_WARNING, "ireq: spurious vector 0x%02X out of range", vector);
        return;
    }

    handler = ireq_table[vector].handler;

    if(handler) {
        handler(vector, frame, ireq_table[vector].privdata);
        return;
    }

    kprintf(KP_WARNING, "ireq: unhandled vector 0x%02X", vector);
}

// SPDX-License-Identifier: BSD-3-Clause

#include <arch/ireq.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/ireq.h>
#include <core/kprintf.h>

#define IREQ_TABLE_SIZE (IREQ_VECTOR_MAX - IREQ_VECTOR_MIN + 1)

struct ireq_entry {
    ireq_handler_type handler;
    void* ctx;
};

static struct ireq_entry ireq_table[IREQ_TABLE_SIZE];

int register_ireq_handler(unsigned int vector, ireq_handler_type handler, void* restrict ctx)
{
    kassert(handler && "invalid IREQ handler function pointer");

    if(vector < IREQ_VECTOR_MIN || vector > IREQ_VECTOR_MAX) {
        return EINVAL;
    }

    ireq_table[vector - IREQ_VECTOR_MIN].handler = handler;
    ireq_table[vector - IREQ_VECTOR_MIN].ctx = ctx;

    return 0;
}

int unregister_ireq_handler(unsigned int vector)
{
    if(vector < IREQ_VECTOR_MIN || vector > IREQ_VECTOR_MAX) {
        return EINVAL;
    }

    ireq_table[vector - IREQ_VECTOR_MIN].handler = NULL;
    ireq_table[vector - IREQ_VECTOR_MIN].ctx = NULL;

    return 0;
}

void dispatch_ireq(unsigned int vector, struct interrupt_frame* restrict frame)
{
    ireq_handler_type handler;

    if(vector < IREQ_VECTOR_MIN || vector > IREQ_VECTOR_MAX) {
        kprintf(KP_WARNING, "ireq: spurious vector 0x%02X out of range", vector);
        return;
    }

    handler = ireq_table[vector - IREQ_VECTOR_MIN].handler;

    if(handler) {
        handler(vector, frame, ireq_table[vector - IREQ_VECTOR_MIN].ctx);
        return;
    }

    kprintf(KP_WARNING, "ireq: unhandled vector 0x%02X", vector);
}

void init_ireq(void)
{
    size_t i;

    for(i = 0; i < IREQ_TABLE_SIZE; ++i) {
        ireq_table[i].handler = NULL;
        ireq_table[i].ctx = NULL;
    }
}

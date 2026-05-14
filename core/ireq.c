#include <core/arch.ireq.h>
#include <core/errno.h>
#include <core/ireq.h>
#include <core/kprintf.h>

#define IREQ_TABLE_SIZE (IREQ_VECTOR_MAX - IREQ_VECTOR_MIN + 1)

static ireq_handler_t ireq_table[IREQ_TABLE_SIZE];

int register_ireq_handler(unsigned int vector, ireq_handler_t handler)
{
    if(vector < IREQ_VECTOR_MIN || vector > IREQ_VECTOR_MAX) {
        return EINVAL;
    }

    ireq_table[vector - IREQ_VECTOR_MIN] = handler;

    return 0;
}

int unregister_ireq_handler(unsigned int vector)
{
    if(vector < IREQ_VECTOR_MIN || vector > IREQ_VECTOR_MAX) {
        return EINVAL;
    }

    ireq_table[vector - IREQ_VECTOR_MIN] = NULL;

    return 0;
}

void dispatch_ireq(unsigned int vector, struct interrupt_frame* restrict frame)
{
    ireq_handler_t handler;

    if(vector < IREQ_VECTOR_MIN || vector > IREQ_VECTOR_MAX) {
        kprintf(KP_WARNING, "ireq: spurious vector 0x%02X out of range", vector);
        return;
    }

    handler = ireq_table[vector - IREQ_VECTOR_MIN];

    if(handler) {
        handler(vector, frame);
        return;
    }

    kprintf(KP_WARNING, "ireq: unhandled vector 0x%02X", vector);
}

void init_ireq(void)
{
    size_t i;

    for(i = 0; i < IREQ_TABLE_SIZE; ++i) {
        ireq_table[i] = NULL;
    }
}

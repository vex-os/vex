#include <arch/ireq.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/ireq.h>
#include <core/kprintf.h>
#include <limits.h>

#define IREQ_TABLE_SIZE (ARCH_IREQ_MAX - ARCH_IREQ_BASE + 1)

struct ireq_entry {
    ireq_handler_type handler;
    void* ctxdata;
};

static struct ireq_entry ireq_table[IREQ_TABLE_SIZE] = { 0 };

unsigned register_ireq_handler(unsigned desired_number, ireq_handler_type handler, void* restrict ctxdata)
{
    unsigned number;

    if(desired_number < IREQ_TABLE_SIZE && ireq_table[desired_number].handler == nullptr) {
        ireq_table[desired_number].handler = handler;
        ireq_table[desired_number].ctxdata = ctxdata;
        return desired_number;
    }

    for(number = 0; number < IREQ_TABLE_SIZE; number += 1) {
        if(ireq_table[number].handler == nullptr) {
            ireq_table[number].handler = handler;
            ireq_table[number].ctxdata = ctxdata;
            return number;
        }
    }

    return UINT_MAX;
}

void unregister_ireq_handler(unsigned number)
{
    ireq_table[number].handler = nullptr;
    ireq_table[number].ctxdata = nullptr;
}

void dispatch_ireq(unsigned number, struct interrupt_frame* restrict frame)
{
    ireq_handler_type handler;

    if(number >= IREQ_TABLE_SIZE) {
        pr_warning("ireq: spurious IRQ 0x%02X out of range", number);
        return;
    }

    handler = ireq_table[number].handler;

    if(handler) {
        handler(number, frame, ireq_table[number].ctxdata);
        return;
    }

    pr_warning("ireq: unhandled IRQ 0x%02X", number);
}

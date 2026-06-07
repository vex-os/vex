#include <limits.h>
#include <sys.arch/ireq.h>
#include <sys/assert.h>
#include <sys/errno.h>
#include <sys/ireq.h>
#include <sys/kprintf.h>

#define IREQ_TABLE_SIZE (ARCH_IREQ_MAX - ARCH_IREQ_BASE + 1)

struct ireq_entry {
    ireq_handler_type handler;
    void* ctxdata;
};

static struct ireq_entry ireq_table[IREQ_TABLE_SIZE] = { 0 };

unsigned add_ireq_handler(unsigned desired_number, ireq_handler_type handler, void* restrict ctxdata)
{
    if(desired_number < IREQ_TABLE_SIZE && ireq_table[desired_number].handler == nullptr) {
        ireq_table[desired_number].handler = handler;
        ireq_table[desired_number].ctxdata = ctxdata;
        return desired_number;
    }

    for(unsigned number = 0; number < IREQ_TABLE_SIZE; number += 1) {
        if(ireq_table[number].handler == nullptr) {
            ireq_table[number].handler = handler;
            ireq_table[number].ctxdata = ctxdata;
            return number;
        }
    }

    return UINT_MAX;
}

void remove_ireq_handler(unsigned number)
{
    ireq_table[number].handler = nullptr;
    ireq_table[number].ctxdata = nullptr;
}

void dispatch_ireq(unsigned number, struct hart_regs* restrict regs)
{
    if(number >= IREQ_TABLE_SIZE) {
        pr_verbose("ireq: spurious IRQ 0x%02X out of range", number);
        return;
    }

    ireq_handler_type handler = ireq_table[number].handler;

    if(handler) {
        handler(number, regs, ireq_table[number].ctxdata);
        return;
    }

    pr_verbose("ireq: unhandled IRQ 0x%02X", number);
}

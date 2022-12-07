/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/kprintf.h>
#include <kaneru/trace.h>

static struct symtab kernel_symtab = {
    .symbols = __kernel_symbols,
    .next = NULL,
};

/* Ensures that kernel symbols are always traceable
 * no matter what happens, even before other symbol
 * tables could be loaded: panic routine relies on that. */
static struct symtab *symtabs = &kernel_symtab;

void register_symtab(struct symtab *restrict st)
{
    if(st) {
        st->next = symtabs;
        symtabs = st;
    }
}

bool trace_address(uintptr_t address, struct symbol *restrict sym, uintptr_t *restrict offset)
{
    size_t i;
    struct symbol psym;
    const struct symtab *st;

    for(st = symtabs; st; st = st->next) {
        psym = st->symbols[0];

        for(i = 0; st->symbols[i].address; i++) {
            if(st->symbols[i].address >= address) {
                *offset = address - psym.address;
                *sym = psym;
                return true;
            }

            psym = st->symbols[i];
        }
    }

    return false;
}

void print_stack_backtrace(const uintptr_t *base_pointer)
{
    size_t frame;
    uintptr_t next_base;
    uintptr_t return_addr;
    uintptr_t sym_offset;
    struct symbol sym;

    if(!base_pointer) {
        /* get the current stack frame */
        base_pointer = get_base_pointer();
    }

    if(!base_pointer) {
        kputs(KP_UNMASKABLE, "stack backtrace not available!");
        return;
    }

    kputs(KP_UNMASKABLE, "stack backtrace:");
    for(frame = 0;; frame++) {
        next_base = base_pointer[0];
        return_addr = base_pointer[1];
        if(!next_base || !return_addr)
            break;

        if(trace_address(return_addr, &sym, &sym_offset))
            kprintf(KP_UNMASKABLE, " %3zu [%p] %s+%#jX", frame, (const void *)return_addr, sym.name, (uintmax_t)sym_offset);
        else
            kprintf(KP_UNMASKABLE, " %3zu [%p]", frame, (const void *)return_addr);

        base_pointer = (const uintptr_t *)next_base;
    }
}

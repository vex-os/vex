/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <string.h>

const void *get_export(const char *restrict sym)
{
    const symbol_t *esym;
    for(esym = &__exports_begin; esym != &__exports_end; esym++) {
        if(strcmp(esym->name, sym))
            continue;
        return (void *)esym->address;
    }

    return NULL;
}

bool trace_address(uintptr_t address, symbol_t *restrict sym, ptrdiff_t *restrict off)
{
    size_t i;
    symbol_t psym;

    psym = __ksymtab[0];
    for(i = 0; __ksymtab[i].address; i++) {
        if(__ksymtab[i].address >= address) {
            *off = address - psym.address;
            *sym = psym;
            return true;
        }

        psym = __ksymtab[i];
    }

    return false;
}

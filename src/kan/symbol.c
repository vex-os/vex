/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <string.h>

const void *get_export(const char *restrict sym)
{
    const symbol_t *esym;
    for(esym = &exports_begin; esym != &exports_end; esym++) {
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

    psym = ksymtab[0];
    for(i = 0; ksymtab[i].address; i++) {
        if(ksymtab[i].address >= address) {
            *off = address - psym.address;
            *sym = psym;
            return true;
        }

        psym = ksymtab[i];
    }

    return false;
}

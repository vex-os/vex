/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kan/kprintf.h>
#include <kan/symbol.h>

bool trace_address(uintptr_t address, symbol_t *restrict sym, ptrdiff_t *restrict off)
{
    size_t i;
    symbol_t psym;

    psym = kernel_syms[0];
    for(i = 0; kernel_syms[i].address; i++) {
        if(kernel_syms[i].address >= address) {
            *off = address - psym.address;
            *sym = psym;
            return true;
        }

        psym = kernel_syms[i];
    }

    return false;
}

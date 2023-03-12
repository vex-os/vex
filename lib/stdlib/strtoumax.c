/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <stdlib.h>
#include <strtoi.h>

uintmax_t strtoumax(const char *restrict s, const char **restrict endptr, int base)
{
    uintmax_t r;
    strtoi_t ctx = { 0 };
    ctx.base = base;
    ctx.signchar = '+';
    
    strtoi_init(s, &ctx);
    if(!ctx.buf) {
        if(endptr)
            *endptr = NULL;
        return 0;
    }

    ctx.inval = (uintmax_t)(UINTMAX_MAX);
    ctx.limit = (uintmax_t)(UINTMAX_MAX / ctx.base);
    ctx.limdigit = (int)(UINTMAX_MAX % ctx.base);

    r = (uintmax_t)strtoi_run(&ctx);    
    if(endptr)
        *endptr = ctx.buf ? ctx.buf : s;
    return (ctx.signchar == '+') ? r : -r;
}

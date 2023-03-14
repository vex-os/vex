/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <stdlib.h>
#include <strtoi.h>

size_t strtouz(const char *restrict s, const char **restrict endptr, int base)
{
    size_t r;
    strtoi_t ctx = { 0 };
    ctx.base = base;
    ctx.signchar = '+';
    
    strtoi_init(s, &ctx);
    if(!ctx.buf) {
        if(endptr)
            *endptr = NULL;
        return 0;
    }

    ctx.inval = (uintmax_t)(SIZE_MAX);
    ctx.limit = (uintmax_t)(SIZE_MAX / ctx.base);
    ctx.limdigit = (int)(SIZE_MAX % ctx.base);

    r = (size_t)strtoi_run(&ctx);    
    if(endptr)
        *endptr = ctx.buf ? ctx.buf : s;
    return (ctx.signchar == '+') ? r : -r;
}

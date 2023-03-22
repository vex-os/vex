/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <limits.h>
#include <stdlib.h>
#include <strtoi.h>

unsigned long strtoul(const char *restrict s, const char **restrict endptr, int base)
{
    long long r;
    strtoi_t ctx = { 0 };
    ctx.s_base = base;
    ctx.s_signchar = '+';
    
    strtoi_init(s, &ctx);
    if(!ctx.s_buf) {
        if(endptr)
            *endptr = NULL;
        return 0;
    }

    ctx.s_inval = (uintmax_t)(ULONG_MAX);
    ctx.s_limit = (uintmax_t)(ULONG_MAX / ctx.s_base);
    ctx.s_limdigit = (int)(ULONG_MAX % ctx.s_base);

    r = (long long)strtoi_run(&ctx);    
    if(endptr)
        *endptr = ctx.s_buf ? ctx.s_buf : s;
    return (ctx.s_signchar == '+') ? r : -r;
}

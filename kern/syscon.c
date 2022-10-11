/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/kprintf.h>
#include <kaneru/syscon.h>

static struct syscon *cur_console = NULL;
static struct syscon *console_list = NULL;

void syscon_bind(struct syscon *console)
{
    cur_console = console;
    kprintf_flush_syscon(cur_console);
}

void syscon_register(struct syscon *console)
{
    struct syscon *it;
    for(it = console_list; it; it = it->next) {
        if(it != console)
            continue;
        return;
    }

    console->next = console_list;
    console_list = console;
}

void syscon_write(const void *s, size_t n)
{
    if(!cur_console || !cur_console->write)
        return;
    cur_console->write(cur_console, s, n);
}

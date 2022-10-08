/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/kprintf.h>
#include <kaneru/syscon.h>

static struct syscon *cur_console = NULL;
static struct syscon *console_list = NULL;

void register_syscon(struct syscon *console)
{
    struct syscon *it;
    for(it = console_list; it; it = it->next) {
        if(it == console) {
            pr_warn("syscon: register_syscon for %s is called twice", console->name);
            return;
        }
    }

    if(console->flags & SYSCON_AUTO_BIND)
        cur_console = console;
    if(console->flags & SYSCON_AUTO_FLUSH)
        kprintf_flush_syscon(console);
    
    console->next = console_list;
    console_list = console;
}

void bind_syscon(struct syscon *console)
{
    cur_console = console;
}

void write_syscon(const void *s, size_t n)
{
    if(!cur_console || !cur_console->write)
        return;
    cur_console->write(cur_console, s, n);
}

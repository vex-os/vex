/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/errno.h>
#include <kaneru/kprintf.h>
#include <kaneru/syscon.h>
#include <stdlib.h>
#include <string.h>

static struct sys_console *console_head = NULL;
static struct sys_console *active_console = NULL;

int register_console(struct sys_console *console)
{
    int errnum;
    struct sys_console *it;
    for(it = console_head; it; it = it->next) {
        if(it != console)
            continue;
        return -EBUSY;
    }

    if(console->init_fn) {
        errnum = console->init_fn(console);
        if(errnum == 0) {
            kp_flush_buffer(console);
            console->next = console_head;
            console_head = console;
            return 0;
        }

        return errnum;
    }

    return 0;
}

void set_active_console(struct sys_console *console)
{
    active_console = console;
}

void console_write(const void *restrict s, size_t n)
{
    struct sys_console *it;
    for(it = console_head; it; it = it->next) {
        if(it->write_fn) {
            it->write_fn(it, s, n);
            continue;
        }
    }
}

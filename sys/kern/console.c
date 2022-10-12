/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <sys/console.h>
#include <sys/klog.h>

static struct console *console_head = NULL;
static struct console *active_console = NULL;

void register_console(struct console *con)
{
    struct console *it;
    for(it = console_head; it; it = it->next) {
        if(it == con) {
            klog(KL_CONSOLE, "%s is already registered", con->name);
            return;
        }
    }

    /* undone: init_fn returns an error code */
    if(con->init_fn && con->init_fn(con) == 0) {
        klog_print_all(con);
        con->next = console_head;
        console_head = con;
        return;
    }
}

void set_active_console(struct console *con)
{
    active_console = con;
}

size_t console_write(const void *s, size_t n)
{
    struct console *it;
    for(it = console_head; it; it = it->next) {
        if(it->write_fn) {
            it->write_fn(it, s, n);
            continue;
        }
    }

    return n;
}

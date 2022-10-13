/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <sys/console.h>
#include <sys/errno.h>
#include <sys/klog.h>

static struct console *console_head = NULL;
static struct console *active_console = NULL;

void register_console(struct console *con)
{
    int errnum;
    struct console *it;
    for(it = console_head; it; it = it->next) {
        if(it == con) {
            klog(KL_CONSOLE, "%s: console already present", con->name);
            return;
        }
    }

    if(con->init_fn) {
        errnum = con->init_fn(con);
        if(errnum == EOK) {
            klog_print_all(con);
            con->next = console_head;
            console_head = con;
        }
        else {
            klog(KL_CONSOLE, "%s: %s", con->name, kstrerror(errnum));
            return;
        }
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

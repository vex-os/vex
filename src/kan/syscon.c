/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <kan/kprintf.h>
#include <kan/syscon.h>
#include <string.h>

static syscon_t *syscon_drivers = NULL;

int register_syscon(syscon_t *restrict con)
{
    int r;
    char *sptr;
    syscon_t *bcon;

    for(bcon = syscon_drivers; bcon; bcon = bcon->next) {
        if(bcon == con || !strcmp(bcon->name, con->name)) {
            pr_warning(KP_SYSCON, "syscon: %s is already registered", con->name);
            return -EBUSY;
        }
    }

    if(!con->write_fn) {
        pr_warning(KP_SYSCON, "syscon: %s: write callback is null", con->name);
        /* The driver can assign the write callback later... */
        /* return -EINVAL; */
    }

    pr_inform(KP_SYSCON, "syscon: registering %s", con->name);

    if(con->init_fn && ((r = con->init_fn(con)) < 0)) {
        pr_warning(KP_SYSCON, "syscon: %s: %s", con->name, strerror(r));
        return r;
    }

    if(con->write_fn) {
        sptr = kp_ring + kp_ring_pos;
        con->write_fn(con, sptr, strnlen(sptr, KP_RING_SIZE - kp_ring_pos));
        con->write_fn(con, kp_ring, strnlen(kp_ring, kp_ring_pos));
    }

    con->next = syscon_drivers;
    syscon_drivers = con;

    return 0;
}

void unregister_syscon(syscon_t *restrict con)
{
    syscon_t *a;
    syscon_t *b;

    if(syscon_drivers) {
        if(syscon_drivers == con) {
            syscon_drivers = con->next;
            pr_inform(KP_SYSCON, "syscon: unregistering %s", con->name);
            return;
        }

        for(a = syscon_drivers->next, b = syscon_drivers; a; b = a, a = b->next) {
            if(a == con) {
                b->next = a->next;
                pr_inform(KP_SYSCON, "syscon: unregistering %s", con->name);
                return;
            }
        }
    }

    pr_warning(KP_SYSCON, "syscon: %s not registered", con->name);
}

void syscon_write(const void *restrict s, size_t n)
{
    syscon_t *bcon;
    for(bcon = syscon_drivers; bcon; bcon = bcon->next) {
        if(!bcon->write_fn)
            continue;
        bcon->write_fn(bcon, s, n);
    }
}

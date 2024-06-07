/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_CONSOLE_H
#define _INCLUDE_SYS_CONSOLE_H
#include <stddef.h>
#include <sys/cdefs.h>

#define CS_PRINTBUFFER  0x0001
#define CS_BLANKED      0x1000

struct console;

typedef void (*cons_write_t)(struct console *restrict cons, const void *restrict buf, size_t sz);
typedef void (*cons_unblank_t)(struct console *restrict cons);

struct console {
    cons_write_t cs_write;
    cons_unblank_t cs_unblank;
    struct console *cs_next;
    char cs_identity[32];
    unsigned cs_flags;
    void *cs_private;
};

int register_console(struct console *restrict cons);
int unregister_console(struct console *restrict cons);
void console_write_all(const void *restrict buf, size_t sz);
void console_unblank_all(void);

#endif /* _INCLUDE_SYS_CONSOLE_H */

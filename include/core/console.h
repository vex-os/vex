/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_CORE_CONSOLE_H
#define INCLUDE_CORE_CONSOLE_H
#include <core/compiler.h>
#include <stddef.h>

#define CS_PRINTBUFFER  0x0001U
#define CS_BLANKED      0x1000U

struct console {
    struct console *cs_next;
    void (*cs_write)(struct console *restrict con, const void *restrict buf, size_t sz);
    void (*cs_unblank)(struct console *restrict con);
    char cs_identity[32];
    unsigned cs_flags;
    void *cs_private;
};

int register_console(struct console *restrict con);
int unregister_console(struct console *restrict con);
void console_write_all(const void *restrict buf, size_t sz);
void console_unblank_all(void);

#endif /* INCLUDE_CORE_CONSOLE_H */

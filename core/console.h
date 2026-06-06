#ifndef A3252828_5A4D_4592_BD44_063284F9226D
#define A3252828_5A4D_4592_BD44_063284F9226D
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

#define CONSF_NONE    0x0000U ///< No special flags
#define CONSF_RECALL  0x0001U ///< Recall the last kprintf history on unblanking
#define CONSF_BLANKED 0x1000U ///< The driver is blanked and won't receive write calls

struct console;
struct console_ops {
    void (*init)(struct console* restrict cons);
    void (*write)(struct console* restrict cons, const void* restrict data, size_t size);
    void (*unblank)(struct console* restrict cons);
};

struct console_private {
    struct console* next;
};

struct console {
    struct console_private priv;
    const struct console_ops* ops;
    const char* identifier;
    unsigned consflags;
    void* ctxdata;
};

int add_console(struct console* restrict cons);
int remove_console(struct console* restrict cons);
void console_write(const void* restrict data, size_t size);
void console_unblank(void);

#endif /* A3252828_5A4D_4592_BD44_063284F9226D */

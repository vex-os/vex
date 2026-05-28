// SPDX-License-Identifier: BSD-3-Clause
#ifndef A3252828_5A4D_4592_BD44_063284F9226D
#define A3252828_5A4D_4592_BD44_063284F9226D
#pragma once

#include <core/cdefs.h>
#include <stddef.h>

#define CONSF_NONE    0x0000U ///< No special flags
#define CONSF_RECALL  0x0001U ///< Recall the last kprintf history on unblanking
#define CONSF_BLANKED 0x1000U ///< The driver is blanked and won't receive write calls

struct console_driver;
struct console_driver_ops {
    void (*init)(struct console_driver* restrict driver);
    void (*write)(struct console_driver* restrict driver, const void* restrict data, size_t size);
    void (*unblank)(struct console_driver* restrict driver);
};

struct console_driver {
    struct console_driver* next;
    const struct console_driver_ops* ops;
    char identifier[64];
    unsigned consflags;
    void* privdata;
};

extern int register_console_driver(struct console_driver* restrict driver);
extern int unregister_console_driver(struct console_driver* restrict driver);
extern void console_write(const void* restrict data, size_t size);
extern void console_unblank_all(void);

#endif /* A3252828_5A4D_4592_BD44_063284F9226D */

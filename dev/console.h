#ifndef A3252828_5A4D_4592_BD44_063284F9226D
#define A3252828_5A4D_4592_BD44_063284F9226D

#include <core/cdefs.h>

#include <stddef.h>

enum {
    CONSOLEFLAG_EMPTY = 0x0000,  ///< No flags
    CONSOLEFLAG_RECALL = 0x0001, ///< Recall current kprintf buffer upon console driver init
    CONSOLEFLAG_BLANK = 0x1000,  ///< Blanked, the underlying resource is used by a different device driver
};

struct console_driver;
struct console_driver_ops {
    void (*init)(struct console_driver* restrict driver);
    void (*write)(struct console_driver* restrict driver, const void* restrict data, size_t size);
    void (*unblank)(struct console_driver* restrict driver);
};

struct console_driver {
    struct console_driver* next;
    struct console_driver_ops* ops;
    unsigned int flags;
    char identity[64];
    void* privdata;
};

extern int console_register(struct console_driver* restrict driver);
extern int console_unregister(struct console_driver* restrict driver);
extern void console_write(const void* restrict data, size_t size);
extern void console_unblank_all(void);

#endif /* A3252828_5A4D_4592_BD44_063284F9226D */

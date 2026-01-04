#ifndef _VEX_CONSDRV_H
#define _VEX_CONSDRV_H 1

#include <vex/compiler.h>

#include <stddef.h>

#define CONSF_EMPTY   0x0000U ///< Empty flag value
#define CONSF_RECALL  0x0001U ///< Recall current kprintf buffer upon driver init
#define CONSF_BLANKED 0x1000U ///< Blanked, the resource is probably released to a higher-level driver

struct consdrv;
struct consdrv_ops {
    void (*init)(struct consdrv* driver);
    void (*write)(struct consdrv* driver, const void* restrict data, size_t sz);
    void (*unblank)(struct consdrv* driver);
};

struct consdrv {
    struct consdrv* next;
    const struct consdrv_ops* ops;
    unsigned int flags;
    char identity[64];
    void* priv_data;
};

extern int consdrv_add(struct consdrv* driver);
extern int consdrv_remove(struct consdrv* driver);
extern void consdrv_write(const void* restrict data, size_t sz);
extern void consdrv_unblank(void);

#endif

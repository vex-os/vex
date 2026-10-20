#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define CONSF_NONE    0x0000U
#define CONSF_RECALL  0x0001U // Recall the last kprintf history on unblanking
#define CONSF_BLANKED 0x1000U // The driver is blanked and won't receive write calls

typedef struct console_s console_t;
typedef struct console_ops_s console_ops_t;
typedef struct console_private_s console_private_t;

struct console_ops_s {
    void (*init)(console_t* restrict cons);
    void (*write)(console_t* restrict cons, const void* restrict data, size_t size);
    void (*unblank)(console_t* restrict cons);
};

struct console_private_s {
    console_t* next;
};

struct console_s {
    console_private_t priv;
    const console_ops_t* ops;
    const char* name;
    unsigned consflags;
    void* ctxdata;
};

int console_add(console_t* restrict cons);
int console_remove(console_t* restrict cons);
void console_write(const void* restrict data, size_t size);
void console_unblank(void);

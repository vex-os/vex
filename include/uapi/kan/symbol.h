#ifndef _INCLUDE_UAPI_KAN_SYMBOL_H__
#define _INCLUDE_UAPI_KAN_SYMBOL_H__
#include <kan/compiler.h>
#include <stdint.h>

typedef struct symbol_s {
    uintptr_t address;
    const char name[64];
} symbol_t;

#endif /* _INCLUDE_UAPI_KAN_SYMBOL_H__ */

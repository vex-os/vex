#ifndef _VEX_KBASE_H
#define _VEX_KBASE_H 1

#include <vex/compiler.h>

#include <stdint.h>

extern uintptr_t kbase_phys;
extern uintptr_t kbase_virt;

extern void init_kbase(void);

#endif

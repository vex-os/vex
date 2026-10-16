#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define PFRAME_PSLAB 0x0001 // Managed by pslab

struct pframe {
    uint32_t refcount;
    uint32_t flags;
};

struct pframe* pframe_of(uintptr_t phys) __nodiscard;
uintptr_t pframe_phys(const struct pframe* restrict pf) __nodiscard;

void pframe_incref(struct pframe* restrict pf);
bool pframe_decref(struct pframe* restrict pf) __nodiscard;
bool pframe_shared(const struct pframe* restrict pf) __nodiscard;

void pframe_storage(uintptr_t* restrict start, size_t* restrict length);

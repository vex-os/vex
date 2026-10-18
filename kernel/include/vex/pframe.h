#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define PFRAME_PSLAB    0x0001 // Managed by pslab
#define PFRAME_CACHE    0x0002 // Owned by a file cache, owner and index are valid
#define PFRAME_UPTODATE 0x0004 // Holds the file contents, zeroes past the end of file
#define PFRAME_DIRTY    0x0008
#define PFRAME_ACCESSED 0x0010 // Used since reclaim last looked at it

#define PFRAME_DIRTY_SHIFT 24 // Bits 24-31: dirty sectors of a file cache page, the first one lowest

struct fcache;

struct pframe {
    uint32_t refcount;
    uint32_t flags;          // PFRAME; the file cache bits are under owner->lock
    struct fcache* owner;    // Under owner->lock
    uint64_t index;          // Page index within the owner
    struct pframe* lru_prev; // Under the file cache LRU lock
    struct pframe* lru_next;
};

struct pframe* pframe_of(uintptr_t phys) __nodiscard;
uintptr_t pframe_phys(const struct pframe* restrict pf) __nodiscard;

void pframe_incref(struct pframe* restrict pf);
bool pframe_decref(struct pframe* restrict pf) __nodiscard;
bool pframe_shared(const struct pframe* restrict pf) __nodiscard;

void pframe_storage(uintptr_t* restrict start, size_t* restrict length);

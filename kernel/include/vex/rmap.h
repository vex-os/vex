#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct pmap_s pmap_t;
typedef struct pframe_s pframe_t;

typedef struct rmap_entry_s rmap_entry_t;

struct rmap_entry_s {
    rmap_entry_t* next;
    pmap_t* pmap;
    uintptr_t virt;
};

// Mappings of one page table change only under its vmspace lock, while the
// walkers rmap_unmap_all and rmap_protect_all may take them away at any time.
// Each mapping owns a reference to its frame, and nothing here flushes TLBs:
// whoever unmaps or downgrades a mapping runs tlb_shootdown afterwards

/// Maps the frame, which hands the new mapping one of the caller's references to it
int rmap_map(pmap_t* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);

/// @return The frame that was mapped, its mapping's reference now the caller's, or zero when nothing was
uintptr_t rmap_unmap(pmap_t* restrict pmap, uintptr_t virt) __nodiscard;

/// Moves the mapping at virt to another frame, the mapping's reference going with it
/// @retval -EAGAIN A walker took the mapping of old_phys away meanwhile
int rmap_replace(pmap_t* restrict pmap, uintptr_t virt, uintptr_t old_phys, uintptr_t new_phys, unsigned vprot);

/// @retval -EAGAIN The page at virt is no longer the one at phys
int rmap_patch(pmap_t* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);

/// @param sole Set when this mapping is the frame's only one and no file cache owns it, nullptr if unwanted
/// @return The frame mapped at virt with a new reference to it, or zero when nothing is mapped
uintptr_t rmap_pin(pmap_t* restrict pmap, uintptr_t virt, bool* restrict sole) __nodiscard;

/// @return The number of mappings removed, each of whose frame references is now the caller's
size_t rmap_unmap_all(pframe_t* restrict pframe) __nodiscard;

/// @return The number of writable mappings made read-only
size_t rmap_protect_all(pframe_t* restrict pframe) __nodiscard;

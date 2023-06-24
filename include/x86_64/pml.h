#ifndef __INCLUDE_X86_64_PML_H__
#define __INCLUDE_X86_64_PML_H__
#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/vprot.h>

#define X86_PML_ADDRESS 0x000FFFFFFFFFF000
#define X86_PML_PRESENT 0x0000000000000001
#define X86_PML_WRITE   0x0000000000000002
#define X86_PML_USER    0x0000000000000004
#define X86_PML_NOEXEC  0x8000000000000000

#define PML_L1_MASK UINT64_C(0x1FF)
#define PML_L2_MASK UINT64_C(0x1FF)
#define PML_L3_MASK UINT64_C(0x1FF)
#define PML_L4_MASK UINT64_C(0x1FF)
#define PML_L5_MASK UINT64_C(0x1FF)

#define PML_L1_SHIFT UINT64_C(12)
#define PML_L2_SHIFT UINT64_C(21)
#define PML_L3_SHIFT UINT64_C(30)
#define PML_L4_SHIFT UINT64_C(39)
#define PML_L5_SHIFT UINT64_C(48)

#define PML_LEVEL3 1
#define PML_LEVEL4 1
#define PML_LEVEL5 0

#define PML_COUNT 0x200
#define PML_KERN 0x100
#define PML_USER 0x000

#define PML_INVALID 0x0000000000000000

typedef uint64_t vmm_pml_t;

static __always_inline inline bool pml_valid(vmm_pml_t entry)
{
    if(entry & X86_PML_PRESENT)
        return true;
    return false;
}

static __always_inline inline uintptr_t pml_address(vmm_pml_t entry)
{
    return (entry & X86_PML_ADDRESS);
}

static __always_inline inline vmm_pml_t pml_mkentry(uintptr_t address, unsigned short vprot)
{
    vmm_pml_t entry = ((X86_PML_PRESENT | X86_PML_NOEXEC) | (address & X86_PML_ADDRESS));
    if(vprot & VPROT_WRITE)
        entry |= X86_PML_WRITE;
    if(vprot & VPROT_USER)
        entry |= X86_PML_USER;
    if(vprot & VPROT_EXEC)
        entry &= ~X86_PML_NOEXEC;
    return entry;
}

#endif /* __INCLUDE_X86_64_PML_H__ */

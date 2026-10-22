#include <arch/msi.h>
#include <vex/hart.h>

#define MSI_ADDRESS_BASE UINT64_C(0xFEE00000)

void arch_msi_compose(const hart_local_t* restrict hart, unsigned vector, uint64_t* restrict address, uint32_t* restrict data)
{
    *address = MSI_ADDRESS_BASE | ((uint64_t)(hart->arch.lapic_id & 0xFF) << 12);
    *data = vector & 0xFF;
}

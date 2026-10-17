#include <arch/msi.h>
#include <vex/hart.h>

#define MSI_ADDRESS_BASE UINT64_C(0xFEE00000)

void arch_msi_compose(unsigned vector, uint64_t* restrict address, uint32_t* restrict data)
{
    const struct hart_local* bsp = boot_hart();

    uint32_t lapic_id = bsp->arch.lapic_id;

    *address = MSI_ADDRESS_BASE | ((uint64_t)(lapic_id & 0xFF) << 12);
    *data = vector & 0xFF;
}

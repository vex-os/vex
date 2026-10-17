#include <arch/msi.h>
#include <vex/hart.h>

#define MSI_ADDRESS_BASE UINT64_C(0xFEE00000)

void arch_msi_compose(unsigned vector, uint64_t* restrict address, uint32_t* restrict data)
{
    uint32_t lapic_id = 0;

    for(unsigned long i = 0; i < num_harts; ++i) {
        if(harts[i].flags & HART_FLAG_BSP) {
            lapic_id = harts[i].arch.lapic_id;
            break;
        }
    }

    *address = MSI_ADDRESS_BASE | ((uint64_t)(lapic_id & 0xFF) << 12);
    *data = vector & 0xFF;
}

#ifndef F9BD53CB_709F_4825_B6ED_6610E0367CC2
#define F9BD53CB_709F_4825_B6ED_6610E0367CC2

#include <core/cdefs.h>

#include <stdint.h>

#define X86_GDT_NULL_SEGMENT 0x00
#define X86_GDT_KERN_CODE_16 0x01
#define X86_GDT_KERN_DATA_16 0x02
#define X86_GDT_KERN_CODE_32 0x03
#define X86_GDT_KERN_DATA_32 0x04
#define X86_GDT_KERN_CODE_64 0x05
#define X86_GDT_KERN_DATA_64 0x06
#define X86_GDT_USER_CODE_64 0x07
#define X86_GDT_USER_DATA_64 0x08

static __always_inline __nodiscard inline uint16_t x86_gdt_selector(uint16_t index, uint16_t ldt, uint16_t ring)
{
    return ((ring & 4) | ((ldt & 1) << 2) | (index << 3));
}

extern void x86_init_gdt(void);

#endif /* F9BD53CB_709F_4825_B6ED_6610E0367CC2 */

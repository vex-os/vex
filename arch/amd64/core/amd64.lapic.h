#ifndef BA3170A4_508F_41E0_A8E3_FA169AD10E8F
#define BA3170A4_508F_41E0_A8E3_FA169AD10E8F

// Currently a stub (no LAPIC initialization and
// other stuff), though the driver now routes IDT vectors
// assumed to be IRQs into the portable IRQ system

extern void amd64_init_lapic(void);

#endif /* BA3170A4_508F_41E0_A8E3_FA169AD10E8F */

kernel_sources += files('uart16550.c')

if kconfig.get('CONFIG_UART16550_ISA', 'n') == 'y'
    kernel_sources += files('uart16550_isa.c')
endif

if kconfig.get('CONFIG_UART16550_PCI', 'n') == 'y'
    kernel_sources += files('uart16550_pci.c')
endif

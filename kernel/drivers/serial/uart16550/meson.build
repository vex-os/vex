kernel_sources += files(
    'uart16550.c',
    'uart16550_fw.c',
)

if kconfig.get('CONFIG_UART16550_PCI', 'n') == 'y'
    kernel_sources += files('uart16550_pci.c')
endif

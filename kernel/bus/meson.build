kernel_sources += files(
    'device.c',
    'fwnode.c',
    'sysbus.c',
)

if kconfig.get('CONFIG_PCI', 'n') == 'y'
    kernel_sources += files('pci.c')
endif

if kconfig.get('CONFIG_ISA', 'n') == 'y'
    kernel_sources += files('isa.c')
endif

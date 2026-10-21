kernel_sources += files(
    'acpi.c',
    'fwnode.c',
    'gas.c',
    'rsdp.c',
    'uacpi.c',
)

if kconfig.get('CONFIG_PCI', 'n') == 'y'
    kernel_sources += files('pci_root.c')
endif

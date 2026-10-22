if kconfig.get('CONFIG_AHCI', 'n') == 'y'
    kernel_sources += files('ahci.c')
endif

if kconfig.get('CONFIG_PATA', 'n') == 'y'
    kernel_sources += files('pata.c')
endif

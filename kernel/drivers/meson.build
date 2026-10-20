if kconfig.get('CONFIG_AHCI', 'n') == 'y'
    kernel_sources += files('ahci.c')
endif

if kconfig.get('CONFIG_BOOTFB', 'n') == 'y'
    kernel_sources += files('bootfb.c')
endif

if kconfig.get('CONFIG_PS2KBD', 'n') == 'y'
    kernel_sources += files('ps2kbd.c')
endif

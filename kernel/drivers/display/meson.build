if kconfig.get('CONFIG_LIMINEFB', 'n') == 'y'
    kernel_sources += files('limine.c')
endif

if kconfig.get('CONFIG_BOCHSFB', 'n') == 'y'
    kernel_sources += files('bochs.c')
endif

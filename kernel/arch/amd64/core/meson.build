kernel_sources += files(
    'cpuprot.c',
    'gdt.c',
    'lapic.c',
    'pmio.c',
)

if kconfig.get('CONFIG_BXCONS', 'n') == 'y'
    kernel_sources += files('bxcons.c')
endif

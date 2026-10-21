kernel_sources += files(
    'cpucaps.c',
    'cpuprot.c',
    'gdt.c',
    'lapic.c',
)

if kconfig.get('CONFIG_BXCONS', 'n') == 'y'
    kernel_sources += files('bxcons.c')
endif

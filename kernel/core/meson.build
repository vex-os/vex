kernel_sources += files(
    'cmdline.c',
    'console.c',
    'kmsg.c',
    'kprintf.c',
    'panic.c',
    'version.c',
)

if kconfig.get('CONFIG_KASSERT', 'n') == 'y'
    kernel_sources += files('assert.c')
endif

if kconfig.get('CONFIG_FBCONS', 'n') == 'y'
    kernel_sources += files('fbcons.c')
endif

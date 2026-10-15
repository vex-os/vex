kernel_sources += files(
    'file.c',
    'mount.c',
    'path.c',
    'vcache.c',
    'vnode.c',
)

if kconfig.get('CONFIG_DEVFS', 'n') == 'y'
    subdir('devfs')
endif

if kconfig.get('CONFIG_ECHFS', 'n') == 'y'
    subdir('echfs')
endif

if kconfig.get('CONFIG_FAT', 'n') == 'y'
    subdir('fat')
endif

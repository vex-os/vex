kernel_sources += files(
    'block_ata.c',
    'block_atapi.c',
    'block_device.c',
    'block_event.c',
    'block_ptable.c',
    'block_request.c',
    'char_device.c',
    'char_event.c',
    'dev_name.c',
    'fb_device.c',
    'fb_edid.c',
    'kplog.c',
    'serial_device.c',
    'tty_device.c',
    'tty_discipline.c',
    'tty_serial.c',
)

if kconfig.get('CONFIG_PTABLE_GPT', 'n') == 'y'
    kernel_sources += files('ptable_gpt.c')
endif

if kconfig.get('CONFIG_PTABLE_MBR', 'n') == 'y'
    kernel_sources += files('ptable_mbr.c')
endif

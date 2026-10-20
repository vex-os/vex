kernel_sources += files(
    'blk_ata.c',
    'blk_atapi.c',
    'blk_device.c',
    'blk_event.c',
    'blk_ptable.c',
    'blk_request.c',
    'chr_device.c',
    'chr_event.c',
    'dev_name.c',
    'fb_device.c',
)

if kconfig.get('CONFIG_PTABLE_GPT', 'n') == 'y'
    kernel_sources += files('ptable_gpt.c')
endif

if kconfig.get('CONFIG_PTABLE_MBR', 'n') == 'y'
    kernel_sources += files('ptable_mbr.c')
endif

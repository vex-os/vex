if kconfig.get('CONFIG_SERIAL_TEST', 'n') == 'y'
    kernel_sources += files('serial_test.c')
endif

if kconfig.get('CONFIG_UART16550', 'n') == 'y'
    subdir('uart16550')
endif

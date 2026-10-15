boot_image = custom_target('vexsys.boot.img',
    input: [
        limine_efi,
        files('limine.conf'),
        kernel_elf,
    ],
    output: 'vexsys.boot.img',
    command: [find_program('sh'), files('mkimage.sh'), '@OUTPUT@', '@INPUT@'],
    build_by_default: false,
)

alias_target('image', boot_image)

run_target('qemu',
    command: [find_program('sh'), files('../scripts/run-qemu-' + arch + '.sh')],
    env: {'VEX_IMAGE': boot_image.full_path()},
    depends: boot_image,
)

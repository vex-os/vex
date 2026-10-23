project('vex', 'c',
    version: '0.0.2',
    license: 'BSD-3-Clause',
    meson_version: '>=1.4.0',
    default_options: [
        'c_std=gnu2x',
        'warning_level=2',
        'werror=true',
        'optimization=2',
        'b_pie=false',
        'b_staticpic=false',
    ],
)

if not meson.is_cross_build()
    error('the kernel can only be built with a cross file, e.g. meson setup build --cross-file cross/amd64-clang.ini')
endif

# Historically speaking Vex uses a different
# architecture name for x86-64 because it's
# somewhat shorter and has less random shit
arch_names = {
    'riscv64': 'riscv64',
    'aarch64': 'aarch64',
    'x86_64': 'amd64',
}

cpu_family = host_machine.cpu_family()

if cpu_family not in arch_names
    error('unsupported cpu_family: ' + cpu_family)
endif

arch = arch_names[cpu_family]

subdir('scripts/kconfig')
subdir('contrib/limine')
subdir('contrib/uacpi')
subdir('kernel')

if get_option('ktest')
    subdir('ktest')
endif

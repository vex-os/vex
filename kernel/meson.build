kernel_config_h = custom_target('config.h',
    output: 'config.h',
    command: [python, kconfig_gen_h,
        '--kconfig', kconfig_root,
        '--config', kconfig_file,
        '--output', '@OUTPUT@',
    ],
    env: kconfig_env,
    depend_files: kconfig_depends + [kconfig_file],
)

# Regenerated on every build so that UTSNAME_VERSION
# follows the checked out commit, see core/version.c
kernel_utsname_h = vcs_tag(
    input: 'core/utsname.h.in',
    output: 'utsname.h',
    command: ['git', 'rev-parse', '--short', 'HEAD'],
    fallback: 'unknown',
)

kernel_sources = [kernel_config_h, kernel_utsname_h]
kernel_includes = [include_directories('include', '.')]
kernel_link_args = []
kernel_link_depends = []

fs = import('fs')

kernel_c_args = [
    # Every translation unit sees CONFIG_* values without having
    # to include anything, so a forgotten include can't silently
    # turn an option off. The path is relative to the build directory,
    # as Ninja names the generated header: an absolute one ends up in
    # depfiles as a file Ninja doesn't know it generates, and objects
    # only pick a changed .config up on the run after
    '-include', fs.relative_to(kernel_config_h.full_path(), meson.project_build_root()),
    '-D__kernel__',
    '-D__KERNEL__',
    '-DUTSNAME_SYSNAME="Vex"',
    '-DUTSNAME_RELEASE="@0@"'.format(meson.project_version()),
    '-DUTSNAME_MACHINE="@0@"'.format(arch),
    '-DUTSNAME_COMPILER="@0@"'.format(meson.get_compiler('c').get_id()),
    # Keeps __FILE__ relative to the source tree regardless
    # of where the build directory is
    '-fmacro-prefix-map=@0@/='.format(fs.relative_to(meson.project_source_root(), meson.project_build_root())),
    '-fPIE',
    '-ffreestanding',
    '-fno-stack-protector',
    '-fno-stack-check',
    '-fno-lto',
    '-funsigned-char',
    '-Wno-unused-parameter',
    '-Wno-pointer-sign',
    '-Wno-sign-compare',
]

subdir('arch' / arch)

subdir('acpi')
subdir('bus')
subdir('core')
subdir('crypto')
subdir('dev')
subdir('device')
subdir('drivers')
subdir('exec')
subdir('fs')
subdir('init')
subdir('irq')
subdir('libk')
subdir('mm')
subdir('proc')
subdir('time')

kernel_sources += uacpi_sources
kernel_includes += limine_includes
kernel_includes += uacpi_includes

# Objects go into an archive which is then linked by calling the linker
# directly: clang has no bare-metal driver for some targets (amd64 being
# one of them) and would delegate the link to host's gcc otherwise
kernel_lib = static_library('kernel', kernel_sources,
    include_directories: kernel_includes,
    c_args: kernel_c_args,
)

ld = find_program('ld')

kernel_elf = custom_target('vexsys.elf',
    input: kernel_lib,
    output: 'vexsys.elf',
    command: [ld,
        '-nostdlib',
        '-static',
        '-z', 'max-page-size=0x1000',
        '-z', 'text',
        kernel_link_args,
        '--whole-archive', '@INPUT@',
        '-o', '@OUTPUT@',
    ],
    depend_files: kernel_link_depends,
    build_by_default: true,
)

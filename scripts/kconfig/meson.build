fs = import('fs')

python = find_program('python3', native: true)

kconfig_root = meson.project_source_root() / 'Kconfig'
kconfig_file = meson.project_build_root() / '.config'

kconfig_defconfig = get_option('defconfig')

if kconfig_defconfig == ''
    kconfig_defconfig = arch + '_defconfig'
endif

kconfig_defconfig = meson.project_source_root() / 'configs' / kconfig_defconfig

# Vendored copy, see contrib/kconfiglib/README.vex
kconfiglib_dir = meson.project_source_root() / 'contrib' / 'kconfiglib'

kconfig_env = {
    'ARCH': arch,
    'srctree': meson.project_source_root(),
    'KCONFIG_CONFIG': kconfig_file,
    'PYTHONDONTWRITEBYTECODE': '1',
    'PYTHONPATH': kconfiglib_dir,
}

kconfig_sync = run_command(python, files('sync_config.py'),
    '--kconfig', kconfig_root,
    '--defconfig', kconfig_defconfig,
    env: kconfig_env,
    check: true,
)

# Reading the files makes Meson reconfigure whenever
# any of them changes, which re-runs the sync above
kconfig_sources = kconfig_sync.stdout().strip().split('\n')

foreach source : kconfig_sources
    fs.read(source)
endforeach

fs.read(kconfig_defconfig)

kconfig = import('keyval').load(kconfig_file)

kconfig_gen_h = files('gen_config_h.py')
kconfig_depends = files(kconfiglib_dir / 'kconfiglib.py', 'gen_config_h.py') + kconfig_sources

run_target('menuconfig',
    command: [python, files('run_menuconfig.py'), kconfig_root],
    env: kconfig_env,
)

run_target('oldconfig',
    command: [python, files(kconfiglib_dir / 'oldconfig.py'), kconfig_root],
    env: kconfig_env,
)

run_target('listnewconfig',
    command: [python, files(kconfiglib_dir / 'listnewconfig.py'), kconfig_root],
    env: kconfig_env,
)

run_target('defconfig',
    command: [python, files(kconfiglib_dir / 'defconfig.py'), '--kconfig', kconfig_root, kconfig_defconfig],
    env: kconfig_env,
)

run_target('savedefconfig',
    command: [python, files(kconfiglib_dir / 'savedefconfig.py'),
        '--kconfig', kconfig_root,
        '--out', meson.project_build_root() / 'defconfig',
    ],
    env: kconfig_env,
)

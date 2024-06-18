## SPDX-License-Identifier: Zlib

SOURCES += kernel/console.c
SOURCES += kernel/fbcon.c
SOURCES += kernel/initcall.c
SOURCES += kernel/limine.c
SOURCES += kernel/main.c
SOURCES += kernel/panic.c
SOURCES += kernel/pmm.c
SOURCES += kernel/printf.c
SOURCES += kernel/slab.c
SOURCES += kernel/vmm.c

CLEAN0 += ${build_dir}/utsname.c
SOURCES += ${build_dir}/utsname.c

${build_dir}/utsname.c: scripts/gen_utsname.sh ${build_dir}
	${SHELL} scripts/gen_utsname.sh ${RELEASE} ${SYSNAME} > $@

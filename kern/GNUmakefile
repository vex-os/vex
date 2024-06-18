## SPDX-License-Identifier: Zlib

SOURCES += kern/console.c
SOURCES += kern/fbcon.c
SOURCES += kern/initcall.c
SOURCES += kern/limine.c
SOURCES += kern/main.c
SOURCES += kern/panic.c
SOURCES += kern/pmm.c
SOURCES += kern/printf.c
SOURCES += kern/slab.c
SOURCES += kern/vmm.c

CLEAN0 += ${build_dir}/utsname.c
SOURCES += ${build_dir}/utsname.c

${build_dir}/utsname.c: scripts/gen_utsname.sh ${build_dir}
	${SHELL} scripts/gen_utsname.sh ${RELEASE} ${SYSNAME} > $@

## SPDX-License-Identifier: Zlib

SOURCES += src/core/console.c
SOURCES += src/core/fbcon.c
SOURCES += src/core/limine.c
SOURCES += src/core/panic.c
SOURCES += src/core/printf.c

SOURCES += src/mm/pmm.c
SOURCES += src/mm/slab.c
SOURCES += src/mm/vmm.c

SOURCES += src/kmain.c

CLEAN0 += ${build_dir}/version.c
SOURCES += ${build_dir}/version.c

${build_dir}/version.c: src/core/version.sh ${build_dir}
	${SHELL} src/core/version.sh ${SYSNAME} ${RELEASE} > $@

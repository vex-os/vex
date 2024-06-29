## SPDX-License-Identifier: Zlib

SOURCES += kern/console.c
SOURCES += kern/fbcon.c
SOURCES += kern/main.c
SOURCES += kern/panic.c
SOURCES += kern/printf.c

CLEAN0 += ${build_dir}/version.c
SOURCES += ${build_dir}/version.c

${build_dir}/version.c: kern/version.sh ${build_dir}
	${SHELL} kern/version.sh ${SYSNAME} ${RELEASE} > $@

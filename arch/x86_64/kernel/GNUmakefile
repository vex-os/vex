## SPDX-License-Identifier: Zlib

SOURCES += arch/x86_64/kernel/bxcon.c
SOURCES += arch/x86_64/kernel/gdt.c
SOURCES += arch/x86_64/kernel/idt.c

CLEAN1 += ${build_dir}/x86_isrs.S
SOURCES += ${build_dir}/x86_isrs.S

${build_dir}/x86_isrs.S: scripts/gen_x86_isrs.sh ${build_dir}
	${SHELL} scripts/gen_x86_isrs.sh > $@

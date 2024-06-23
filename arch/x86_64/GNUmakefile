## SPDX-License-Identifier: Zlib

CLEAN1 += ${build_dir}/isr_stubs_x86.S
SOURCES += ${build_dir}/isr_stubs_x86.S

${build_dir}/isr_stubs_x86.S: scripts/gen_isr_stubs_x86.sh ${build_dir}
	${SHELL} scripts/gen_isr_stubs_x86.sh > $@

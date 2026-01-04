sources += kernel/acpi.c
sources += kernel/consdrv.c
sources += kernel/devinit.c
sources += kernel/fsinit.c
sources += kernel/hhdm.c
sources += kernel/kbase.c
sources += kernel/kmain.c
sources += kernel/kprintf.c
sources += kernel/limine.c
sources += kernel/madt.c
sources += kernel/memmap.c
sources += kernel/panic.c
sources += kernel/pmm.c
sources += kernel/slab.c
sources += kernel/vmm.c

GENVERSION := ${build_dir}/generated.version.c

sources += ${GENVERSION}
clean_s += ${GENVERSION}

${GENVERSION}: ${build_dir}
	@printf "  GEN\t%s\n" "$@"
	@truncate -s 0 $@
	@printf "// generated at %s UTC\n" "$(date -u +"%Y-%m-%d %H:%M:%S")" >> $@
	@printf "// warning: changes will be lost\n" >> $@
	@printf "const char k_sysname[] = \"%s\";\n" ${SYSNAME} >> $@
	@printf "const char k_release[] = \"%s\";\n" ${RELEASE} >> $@
	@printf "const char k_version[] = \"%s-%s %s\";\n" ${GIT_REV} ${ENVT} "${TIMESTAMP}" >> $@
	@printf "const char k_machine[] = \"%s\";\n" ${ARCH} >> $@

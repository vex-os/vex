## SPDX-License-Identifier: BSD-2-Clause

export LC_ALL=C
export LANGUAGE=C
export LANG=C

ARCH ?= x86_64
ENVT ?= clang
CONFIG ?= default

SYSNAME ?= Vex
RELEASE ?= 0.0.1
GIT_REV := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

PYTHON := $(shell command -v py 2> /dev/null || command -v python3 2> /dev/null || command -v python 2> /dev/null)

include arch/${ARCH}/env.${ENVT}.mk

override CFLAGS += -fPIE
override CFLAGS += -ffreestanding
override CFLAGS += -Wall -Wextra -Werror
override CFLAGS += -Wno-unused-parameter
override CFLAGS += -Wno-pointer-sign
override CFLAGS += -funsigned-char
override CFLAGS += -mcmodel=kernel
override CFLAGS += -O2

override CPPFLAGS += -D __kernel__
override CPPFLAGS += -D __KERNEL__
override CPPFLAGS += -I arch/${ARCH}/include
override CPPFLAGS += -I include

override LDFLAGS += -nostdlib

build_dir := build
output_dir := output

${build_dir}/%.c.o: %.c | ${build_dir}
	@printf "  CC\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${build_dir}/%.S.o: %.S | ${build_dir}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${build_dir}/%.s.o: %.s | ${build_dir}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${build_dir}/%.ld: %.lds | ${build_dir}
	@printf "  CPP\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CPPFLAGS} -E -P -xc -D __ASSEMBLER__ $< > $@

sources :=
sources-0 :=
sources-1 :=

objects :=
objects-0 :=
objects-1 :=

clean_s :=
clean_d :=

ALLDEPS :=
PHONIES :=

ROOT_DIR := ${build_dir}/rootfs

LDSCRIPT := ${build_dir}/arch/${ARCH}/vexsys.ld
KERNEL := ${output_dir}/vexsys.elf

GENCONFIG := ${build_dir}/generated.config.mk

-include ${GENCONFIG}

include arch/${ARCH}/device/GNUmakefile
include arch/${ARCH}/kernel/GNUmakefile

include boot/GNUmakefile

include device/GNUmakefile
include filesys/GNUmakefile
include kernel/GNUmakefile
include libk/GNUmakefile

sources += ${sources-1}
objects += ${objects-1}

objects += ${sources:=.o}
objects := $(patsubst %,${build_dir}/%,$(objects))

clean_s += ${objects}
clean_s += ${LDSCRIPT}
clean_s += ${KERNEL}
clean_s += ${KBOOT}

clean_d += ${ROOT_DIR}
clean_d += ${output_dir}
clean_d += ${build_dir}

PHONIES += all
PHONIES += config
PHONIES += force
PHONIES += clean distclean
PHONIES += kernel

ALLDEPS += kernel

all: ${ALLDEPS}
	@true

config: ${GENCONFIG}
	@true

force:

clean:
	@rm -vrf ${clean_s}

distclean:
	@rm -vrf ${clean_s}
	@rm -vrf ${clean_d}

kernel: ${KERNEL}
	@true

${GENCONFIG}: config/${CONFIG}.conf | scripts/generate_config.py ${build_dir}
	@printf "  CONF\t%s\n" "$@"
	@${PYTHON} scripts/generate_config.py $< $@ ${ARCH}

${KERNEL}: ${LDSCRIPT} ${objects} ${output_dir}
	@printf "  LD\t%s\n" "$@"
	@${LD} ${LDFLAGS} -T ${LDSCRIPT} -o $@ ${objects}

${build_dir}:
	@mkdir -p $@

${output_dir}:
	@mkdir -p $@

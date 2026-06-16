export LC_ALL=C
export LANGUAGE=C
export LANG=C

ALLDEPS :=
PHONIES :=
CLEAN_S :=
CLEAN_D :=

PYTHON := $(shell command -v python3 2> /dev/null || command -v python 2> /dev/null || command -v py 2> /dev/null)

CONFIGURATION ?= default
CONFIGURATION_MK := .configuration.mk
-include ${CONFIGURATION_MK}

ALL_CONFIG_FILES := $(wildcard conf/config.*.conf)
ALL_FILES_TABLES := $(wildcard conf/files.*.lst)

${CONFIGURATION_MK}: configure.py ${ALL_CONFIG_FILES} ${ALL_FILES_TABLES}
	@printf "  CONF\t%s\n" "conf/${CONFIGURATION}.conf"
	@${PYTHON} configure.py ${CONFIGURATION}

ARCH ?= amd64
ENVT ?= clang

SYSNAME ?= Vex
RELEASE ?= 0.0.2

GIT_REV := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

include conf/envt.${ARCH}.${ENVT}.mk

override CFLAGS += -std=gnu23

override CFLAGS += -fPIE
override CFLAGS += -ffreestanding
override CFLAGS += -Wall -Wextra -Werror
override CFLAGS += -Wno-unused-parameter
override CFLAGS += -Wno-pointer-sign -Wno-sign-compare
override CFLAGS += -funsigned-char
override CFLAGS += -mcmodel=kernel
override CFLAGS += -O2

override CPPFLAGS += -D __kernel__
override CPPFLAGS += -D __KERNEL__
override CPPFLAGS += -I arch/${ARCH}/include
override CPPFLAGS += -I include
override CPPFLAGS += -I .

override CPPFLAGS += -D UTSNAME_SYSNAME='"${SYSNAME}"'
override CPPFLAGS += -D UTSNAME_RELEASE='"${RELEASE}"'
override CPPFLAGS += -D UTSNAME_VERSION='"${GIT_REV}-${ENVT}"'
override CPPFLAGS += -D UTSNAME_MACHINE='"${ARCH}"'

override CPPFLAGS += -D ELF32_ARCH=${ELF32_ARCH}
override CPPFLAGS += -D ELF64_ARCH=${ELF64_ARCH}

override LDFLAGS += -nostdlib

BUILD_DIR := build

${BUILD_DIR}/%.c.o: %.c ${CONFIGURATION_MK} | ${BUILD_DIR}
	@printf "  CC\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${BUILD_DIR}/%.S.o: %.S ${CONFIGURATION_MK} | ${BUILD_DIR}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${BUILD_DIR}/%.s.o: %.s ${CONFIGURATION_MK} | ${BUILD_DIR}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${BUILD_DIR}/%.ld: %.lds ${CONFIGURATION_MK} | ${BUILD_DIR}
	@printf "  CPP\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CPPFLAGS} -E -P -xc -D __ASSEMBLER__ $< > $@

LINKER_SCRIPT := ${BUILD_DIR}/conf/link.${ARCH}.ld
KERNEL_BINARY := ${BUILD_DIR}/vexsys.elf

OBJECTS := ${SOURCES}
OBJECTS := $(patsubst %,${BUILD_DIR}/%.o,${OBJECTS})

CLEAN_S += ${OBJECTS}
CLEAN_S += ${LINKER_SCRIPT}
CLEAN_S += ${KERNEL_BINARY}

CLEAN_D += ${BUILD_DIR}
CLEAN_D += ${CONFIGURATION_MK}

PHONIES += all
PHONIES += force
PHONIES += clean distclean
PHONIES += kernel

ALLDEPS += kernel

include GNUmakefile.boot

all: ${ALLDEPS}
	@true

force:

clean:
	@rm -vrf ${CLEAN_S}

distclean:
	@rm -vrf ${CLEAN_S}
	@rm -vrf ${CLEAN_D}

kernel: ${KERNEL_BINARY}
	@true

${KERNEL_BINARY}: ${LINKER_SCRIPT} ${OBJECTS} | ${BUILD_DIR}
	@printf "  LD\t%s\n" "$@"
	@${LD} ${LDFLAGS} -T ${LINKER_SCRIPT} -o $@ ${OBJECTS}

${BUILD_DIR}:
	@mkdir -p $@

.DEFAULT_GOAL := all
.PHONY: ${PHONIES}

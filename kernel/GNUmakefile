export LC_ALL=C.UTF-8
export LANGUAGE=C.UTF-8
export LANG=C.UTF-8

ALLDEPS :=
PHONIES :=
CLEAN_S :=
CLEAN_D :=

PYTHON := $(shell command -v python3 2> /dev/null || command -v python 2> /dev/null || command -v py 2> /dev/null)

ARCH ?= amd64
ENVT ?= clang

CONF ?= default
CONF_MK := .config.${ARCH}.${ENVT}.mk
-include ${CONF_MK}

ALL_CONFIG_FILES := $(wildcard config/*.conf)
ALL_FILES_TABLES := $(wildcard config/*.lst)

${CONF_MK}: configure.py ${ALL_CONFIG_FILES} ${ALL_FILES_TABLES}
	@printf "  CONF\t%s\n" "config/${CONF}.conf"
	@${PYTHON} configure.py ${ARCH} ${ENVT} ${CONF}

SYSNAME ?= Vex
RELEASE ?= 0.0.2

GIT_REV := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

include config/envt.${ARCH}.${ENVT}.mk

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

BUILD_DIR ?= build

${BUILD_DIR}/%.c.o: %.c ${CONF_MK} | ${BUILD_DIR}
	@printf "  CC\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${BUILD_DIR}/%.S.o: %.S ${CONF_MK} | ${BUILD_DIR}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${BUILD_DIR}/%.s.o: %.s ${CONF_MK} | ${BUILD_DIR}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

KERNEL_ELF ?= ${BUILD_DIR}/vexsys.elf

OBJECTS := ${SOURCES}
OBJECTS := $(patsubst %,${BUILD_DIR}/%.o,${OBJECTS})

CLEAN_S += ${OBJECTS}
CLEAN_S += ${KERNEL_ELF}

CLEAN_D += ${BUILD_DIR}
CLEAN_D += ${CONF_MK}

PHONIES += all
PHONIES += force
PHONIES += clean distclean
PHONIES += kernel

ALLDEPS += kernel

all: ${ALLDEPS}
	@true

force:

clean:
	@rm -vrf ${CLEAN_S}

distclean:
	@rm -vrf ${CLEAN_S}
	@rm -vrf ${CLEAN_D}

kernel: ${KERNEL_ELF}
	@true

${KERNEL_ELF}: ${CURDIR}/config/ldscript.${ARCH}.ld ${OBJECTS} | ${BUILD_DIR}
	@printf "  LD\t%s\n" "$@"
	@${LD} ${LDFLAGS} -T ${CURDIR}/config/ldscript.${ARCH}.ld -o $@ ${OBJECTS}

${BUILD_DIR}:
	@mkdir -p $@

.DEFAULT_GOAL := all
.PHONY: ${PHONIES}

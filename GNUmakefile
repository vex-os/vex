export LC_ALL=C
export LANGUAGE=C
export LANG=C

CONFIG ?= default
CONFIG_MK := .configuration.mk

-include ${CONFIG_MK}

GIT_REV := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

ARCH ?= amd64
ENVT ?= clang

SYSNAME ?= Vex
RELEASE ?= 0.0.2

PYTHON := $(shell command -v python3 2> /dev/null || command -v python 2> /dev/null || command -v py 2> /dev/null)

include conf/envt.${ARCH}.${ENVT}.mk

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
override CPPFLAGS += -I arch/${ARCH}/pub
override CPPFLAGS += -I arch/${ARCH}
override CPPFLAGS += -I pub
override CPPFLAGS += -I .

override CPPFLAGS += -D UTSNAME_SYSNAME='"${SYSNAME}"'
override CPPFLAGS += -D UTSNAME_RELEASE='"${RELEASE}"'
override CPPFLAGS += -D UTSNAME_VERSION='"${GIT_REV} ${ENVT}"'
override CPPFLAGS += -D UTSNAME_MACHINE='"${ARCH}"'

override LDFLAGS += -nostdlib

BUILD_DIR ?= build
OUTPUT_DIR ?= output

.configuration.mk: conf/config.${CONFIG}.conf Configure.py
	@printf "  CONF\t%s\n" "conf/${CONFIG}.conf"
	@${PYTHON} Configure.py ${CONFIG}

${BUILD_DIR}/%.c.o: %.c ${CONFIG_MK} | ${BUILD_DIR}
	@printf "  CC\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${BUILD_DIR}/%.S.o: %.S ${CONFIG_MK} | ${BUILD_DIR}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${BUILD_DIR}/%.s.o: %.s ${CONFIG_MK} | ${BUILD_DIR}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${BUILD_DIR}/%.ld: %.lds ${CONFIG_MK} | ${BUILD_DIR}
	@printf "  CPP\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CPPFLAGS} -E -P -xc -D __ASSEMBLER__ $< > $@

ALLDEPS :=
PHONIES :=
ROOT_DIR := ${BUILD_DIR}/rootfs

LDSCRIPT := ${BUILD_DIR}/conf/link.${ARCH}.ld
KERNEL := ${OUTPUT_DIR}/vexsys.elf

OBJECTS := ${SOURCES}
OBJECTS := $(patsubst %,${BUILD_DIR}/%.o,$(OBJECTS))

CLEAN_S += ${OBJECTS}
CLEAN_S += ${LDSCRIPT}
CLEAN_S += ${KERNEL}
CLEAN_S += ${KBOOT}

CLEAN_D += ${ROOT_DIR}
CLEAN_D += ${OUTPUT_DIR}
CLEAN_D += ${BUILD_DIR}
CLEAN_D += ${CONFIG_MK}

PHONIES += all
PHONIES += config
PHONIES += force
PHONIES += clean distclean
PHONIES += kernel

ALLDEPS += kernel

include GNUmakefile.boot

all: ${ALLDEPS}
	@true

config: ${GENCONFIG}
	@true

force:

clean:
	@rm -vrf ${CLEAN_S}

distclean:
	@rm -vrf ${CLEAN_S}
	@rm -vrf ${CLEAN_D}

kernel: ${KERNEL}
	@true

${KERNEL}: ${LDSCRIPT} ${OBJECTS} ${OUTPUT_DIR}
	@printf "  LD\t%s\n" "$@"
	@${LD} ${LDFLAGS} -T ${LDSCRIPT} -o $@ ${OBJECTS}

${BUILD_DIR}:
	@mkdir -p $@

${OUTPUT_DIR}:
	@mkdir -p $@

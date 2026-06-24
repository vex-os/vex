export LC_ALL=C.UTF-8
export LANGUAGE=C.UTF-8
export LANG=C.UTF-8

PYTHON := $(shell command -v python3 2> /dev/null || command -v python 2> /dev/null || command -v py 2> /dev/null)

ARCH ?= amd64
ENVT ?= clang

INTERM_DIR ?= build
OUTPUT_DIR ?= output

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

${INTERM_DIR}/%.c.o: %.c ${CONF_MK} | ${INTERM_DIR}
	@printf "  CC\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${INTERM_DIR}/%.S.o: %.S ${CONF_MK} | ${INTERM_DIR}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${INTERM_DIR}/%.s.o: %.s ${CONF_MK} | ${INTERM_DIR}
	@printf "  AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

KERNEL ?= ${OUTPUT_DIR}/vexsys.elf

HEADERS_SRC :=
HEADERS_SRC += $(shell find include/usr -type f -name '*.h')
HEADERS_SRC += $(shell find arch/${ARCH}/include/usr -type f -name '*.h')

HEADERS_DIR := ${OUTPUT_DIR}/include

HEADERS_DST := $(HEADERS_SRC:include/usr/%=${HEADERS_DIR}/%)
HEADERS_DST := $(HEADERS_DST:arch/${ARCH}/include/usr/%=${HEADERS_DIR}/%)

vpath %.h include/usr arch/${ARCH}/include/usr

OBJECTS := ${SOURCES}
OBJECTS := $(patsubst %,${INTERM_DIR}/%.o,${OBJECTS})

CLEAN_S += ${OBJECTS}
CLEAN_S += ${KERNEL}

CLEAN_D += ${INTERM_DIR}
CLEAN_D += ${CONF_MK}

PHONIES :=
PHONIES += all
PHONIES += clean
PHONIES += binary
PHONIES += headers

all: binary headers
	@true

clean:
	@rm -vrf ${INTERM_DIR}
	@rm -vf ${KERNEL}
	@rm -vf ${CONF_MK}

binary: ${KERNEL}
	@true

headers: ${HEADERS_DST}
	@true

${KERNEL}: config/ldscript.${ARCH}.ld ${OBJECTS}
	@printf "  LD\t%s\n" "$@"
	@${LD} ${LDFLAGS} -T config/ldscript.${ARCH}.ld -o $@ ${OBJECTS}

${HEADERS_DIR}/%: %
	@printf "  HDR\t%s\n" "$@"
	@mkdir -p $(dir $@)
	@cp -f $< $@

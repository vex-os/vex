export LC_ALL=C
export LANGUAGE=C
export LANG=C

ARCH ?= amd64
ENVT ?= clang

INTERM_DIR := build
OUTPUT_DIR := output

PHONIES :=
PHONIES += all
PHONIES += clean
PHONIES += kernel-all kernel-clean kernel-binary kernel-headers
PHONIES += bootimg-all bootimg-clean

.PHONY: ${PHONIES}

all: kernel-all bootimg-all
	@true

clean: kernel-clean bootimg-clean
	@rm -vrf ${INTERM_DIR} ${OUTPUT_DIR}

kernel-all: kernel-binary kernel-headers
	@true

kernel-clean:
	@${MAKE} -C kernel INTERM_DIR=../${INTERM_DIR}/kernel OUTPUT_DIR=../${OUTPUT_DIR} clean

kernel-binary:
	@test -d ${OUTPUT_DIR} || mkdir -p ${OUTPUT_DIR}
	@test -d ${INTERM_DIR}/kernel || mkdir -p ${INTERM_DIR}/kernel
	@${MAKE} -C kernel INTERM_DIR=../${INTERM_DIR}/kernel OUTPUT_DIR=../${OUTPUT_DIR} binary

kernel-headers: kernel-binary
	@test -d ${INTERM_DIR}/kernel || mkdir -p ${INTERM_DIR}/kernel
	@test -d ${INTERM_DIR}/kernel || mkdir -p ${INTERM_DIR}/kernel
	@${MAKE} -C kernel INTERM_DIR=../${INTERM_DIR}/kernel OUTPUT_DIR=../${OUTPUT_DIR} headers

bootimg-all: kernel-all
	@test -d ${INTERM_DIR}/boot || mkdir -p ${INTERM_DIR}/boot
	@test -d ${INTERM_DIR}/boot || mkdir -p ${INTERM_DIR}/boot
	@${MAKE} -C boot INTERM_DIR=../${INTERM_DIR}/boot OUTPUT_DIR=../${OUTPUT_DIR} all

bootimg-clean:
	@${MAKE} -C boot INTERM_DIR=../${INTERM_DIR}/boot OUTPUT_DIR=../${OUTPUT_DIR} clean

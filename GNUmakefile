export LC_ALL=C
export LANGUAGE=C
export LANG=C

ARCH ?= amd64
ENVT ?= clang

BUILD_DIR := ${CURDIR}/build

.PHONY: all clean distclean kernel bootimg

all: kernel bootimg
	@true

clean:
	@${MAKE} -C ${CURDIR}/kernel BUILD_DIR=${BUILD_DIR}/kernel clean

distclean:
	@${MAKE} -C ${CURDIR}/kernel BUILD_DIR=${BUILD_DIR}/kernel distclean
	@${MAKE} -C ${CURDIR}/boot BUILD_DIR=${BUILD_DIR}/bootimg distclean
	@rm -vrf ${BUILD_DIR}

kernel:
	@test -d ${BUILD_DIR}/kernel || mkdir -p ${BUILD_DIR}/kernel
	@${MAKE} -C ${CURDIR}/kernel BUILD_DIR=${BUILD_DIR}/kernel all

bootimg: kernel
	@test -d ${BUILD_DIR}/bootimg || mkdir -p ${BUILD_DIR}/bootimg
	@${MAKE} -C ${CURDIR}/boot BUILD_DIR=${BUILD_DIR}/bootimg all

MAKEFLAGS += -rR

BUILD_DIR	?= build
CROSS		?= amd64-clang
NINJA		?= ninja

.DEFAULT_GOAL := all

${BUILD_DIR}/build.ninja:
	meson setup ${BUILD_DIR} --cross-file cross/${CROSS}.ini

all: ${BUILD_DIR}/build.ninja FORCE
	${NINJA} -C ${BUILD_DIR}

clean:
	if [ -f ${BUILD_DIR}/build.ninja ]; then ${NINJA} -C ${BUILD_DIR} clean; fi

distclean:
	rm -vrf ${BUILD_DIR}

%: ${BUILD_DIR}/build.ninja FORCE
	${NINJA} -C ${BUILD_DIR} $@

GNUmakefile: ;
FORCE: ;

.PHONY: all clean distclean FORCE

#!/bin/sh
# Usage: scripts/ktest.sh [slow] [verbose] [log number]
# Builds the test image and boots it in QEMU. The arguments go to ktest/qemu.<arch>.sh
set -eu

cd "$(dirname "$(realpath "$0")")/.."

build_dir="${BUILD_DIR:-build}"
arch="${ARCH:-amd64}"

sh scripts/build.sh ktest-image

VEX_IMAGE="${build_dir}/ktest/vexsys.boot.img" exec sh "ktest/qemu.${arch}.sh" "$@"

#!/bin/sh
# Usage: scripts/build.sh [ninja arguments or targets]
set -eu

cd "$(dirname "$(realpath "$0")")/.."

build_dir="${BUILD_DIR:-build}"

if ! test -f "${build_dir}/build.ninja"; then
    echo "build: ${build_dir} is not configured, run scripts/configure.sh first" >&2
    exit 1
fi

exec ninja -C "${build_dir}" "$@"

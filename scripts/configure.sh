#!/bin/sh
# Usage: scripts/configure.sh [-b build_dir] [-t clang|gcc] [-a arch] [-d defconfig] [-k] [-w]
set -eu

cd "$(dirname "$(realpath "$0")")/.."

build_dir="${BUILD_DIR:-build}"
toolchain=clang
arch=amd64
defconfig=
ktest=true
wipe=false

while getopts "b:t:a:d:kwh" option; do
    case "${option}" in
        b) build_dir="${OPTARG}" ;;
        t) toolchain="${OPTARG}" ;;
        a) arch="${OPTARG}" ;;
        d) defconfig="${OPTARG}" ;;
        k) ktest=false ;;
        w) wipe=true ;;
        *)
            echo "Usage: $0 [-b build_dir] [-t clang|gcc] [-a arch] [-d defconfig] [-k no ktest] [-w wipe]" >&2
            exit 2
            ;;
    esac
done

cross_file="cross/${arch}-${toolchain}.ini"

if ! test -f "${cross_file}"; then
    echo "configure: no cross file ${cross_file}" >&2
    exit 1
fi

for tool in meson ninja; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "configure: ${tool} is not installed" >&2
        exit 1
    fi
done

args="--cross-file ${cross_file} -Dktest=${ktest}"

if test -n "${defconfig}"; then
    args="${args} -Ddefconfig=${defconfig}"
fi

if test -f "${build_dir}/build.ninja"; then
    if "${wipe}"; then
        args="${args} --wipe"
    else
        args="${args} --reconfigure"
    fi
fi

# shellcheck disable=SC2086
exec meson setup "${build_dir}" ${args}

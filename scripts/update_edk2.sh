#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

# Make sure the script is ran from the
# source tree root (we use absolute paths)
cd $(dirname $(realpath $(dirname ${0})))

download() { wget -q --show-progress -O "${PWD}/${2}/${3}" "${1}/${3}"; }

ovmf_url="https://retrage.github.io/edk2-nightly/bin"
download ${ovmf_url} arch/riscv64/boot RELEASERISCV64_VIRT.fd
download ${ovmf_url} arch/x86_64/boot RELEASEX64_OVMF.fd

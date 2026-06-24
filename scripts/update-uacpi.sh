#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

archive_url="https://github.com/uACPI/uACPI/archive/refs/heads/master.tar.gz"

temp_file="$(mktemp /tmp/uacpi-XXXXXX.tar.gz)"
temp_dir="$(mktemp -d /tmp/uacpi-XXXXXX)"

function cleanup {
    rm -fv ${temp_file}
    rm -frv ${temp_dir}
}

trap cleanup EXIT

${SHELL} scripts/download-naive.sh ${temp_file} ${archive_url} || exit 1

tar -xzf ${temp_file} -C ${temp_dir} || exit 1

rm -frv kernel/include/uacpi uacpi || exit 1
mkdir -p kernel/include/uacpi uacpi || exit 1
mkdir -p kernel/lib/uacpi || exit 1

src_dir="$(find ${temp_dir} -maxdepth 1 -mindepth 1 -type d | head -n1)"

cp -rv ${src_dir}/include/uacpi/. kernel/include/uacpi/ || exit 1
find ${src_dir}/source -name '*.c' -exec cp -v {} kernel/lib/uacpi/ \; || exit 1

exit 0

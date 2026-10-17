#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd "$(dirname "$(realpath "${0}")")" || exit 1

archive_url="https://github.com/uACPI/uACPI/archive/refs/heads/master.tar.gz"

temp_file="$(mktemp /tmp/uacpi-XXXXXX.tar.gz)"
temp_dir="$(mktemp -d /tmp/uacpi-XXXXXX)"

cleanup() {
    rm -fv ${temp_file}
    rm -frv ${temp_dir}
}

trap cleanup EXIT

sh ../download.sh ${temp_file} ${archive_url} || exit 1

tar -xzf ${temp_file} -C ${temp_dir} || exit 1

src_dir="$(find ${temp_dir} -maxdepth 1 -mindepth 1 -type d | head -n1)"

# Only the vendored directories are replaced, meson.build
# and README.vex next to them are maintained by hand
rm -frv include source || exit 1
mkdir -p include/uacpi source || exit 1

cp -rv ${src_dir}/include/uacpi/. include/uacpi/ || exit 1
find ${src_dir}/source -name '*.c' -exec cp -v {} source/ \; || exit 1
cp -v ${src_dir}/LICENSE LICENSE || exit 1

exit 0

#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

downloader=

if command -v wget > /dev/null 2>&1; then
    downloader="wget -q --show-progress -O"
elif command -v curl > /dev/null 2>&1; then
    downloader="curl -L -o"
else
    >&2 printf "error: no download tool found\n"
    exit 1
fi

archive_url="https://github.com/uACPI/uACPI/archive/refs/heads/master.tar.gz"

tmpfile="$(mktemp /tmp/uacpi-XXXXXX.tar.gz)"
tmpdir="$(mktemp -d /tmp/uacpi-XXXXXX)"

cleanup() {
    rm -f "${tmpfile}"
    rm -rf "${tmpdir}"
}
trap cleanup EXIT

${downloader} "${tmpfile}" "${archive_url}"

tar -xzf "${tmpfile}" -C "${tmpdir}"

rm -rfv ext/uacpi
mkdir -p ext/uacpi

srcdir="$(find "${tmpdir}" -maxdepth 1 -mindepth 1 -type d | head -n1)"

cp -rv "${srcdir}/include/uacpi/." ext/uacpi/
find "${srcdir}/source" -name '*.c' -exec cp -v {} ext/uacpi/ \;

exit 0

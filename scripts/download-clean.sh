#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

test -z "${1}" && { >&2 printf "error: missing destination file\n"; exit 1; }
test -z "${2}" && { >&2 printf "error: missing source URL\n"; exit 1; }

cd $(dirname $(realpath $(dirname ${0})))

rm -fv ${1} || exit 1

${SHELL} scripts/download-naive.sh ${1} ${2} || exit 1

exit 0

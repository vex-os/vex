#!/bin/sh
# Usage: download.sh <destination> <url>
# Replaces the destination file with the one downloaded from the URL

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

test -z "${1}" && { >&2 printf "error: missing destination file\n"; exit 1; }
test -z "${2}" && { >&2 printf "error: missing source URL\n"; exit 1; }

downloader=

if command -v wget > /dev/null 2>&1; then
    downloader="wget -q --show-progress -O"
elif command -v curl > /dev/null 2>&1; then
    downloader="curl -L -o"
else
    >&2 printf "error: no download tool found\n"
    exit 1
fi

rm -f "${1}" || exit 1

exec ${downloader} "${1}" "${2}" || exit 1

exit 0

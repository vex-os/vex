#!/bin/sh
cd $(dirname $(dirname ${0}))
command -v clang-format >/dev/null 2>&1 || { >&2 echo "clang-format is not installed. Aborting."; exit 1; }

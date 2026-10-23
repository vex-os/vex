#!/bin/sh
# Usage: scripts/format.sh [-c] [-g]
# Formats kernel/ and ktest/ with clang-format-20. -c only checks, -g takes only the files git reports as changed
set -eu

cd "$(dirname "$(realpath "$0")")/.."

formatter="${CLANG_FORMAT:-clang-format-20}"
check=false
changed=false

while getopts "cgh" option; do
    case "${option}" in
        c) check=true ;;
        g) changed=true ;;
        *)
            echo "Usage: $0 [-c check] [-g changed files only]" >&2
            exit 2
            ;;
    esac
done

if ! command -v "${formatter}" >/dev/null 2>&1; then
    echo "format: ${formatter} is not installed" >&2
    exit 1
fi

if "${changed}"; then
    files=$(git ls-files -m -o --exclude-standard -- kernel ktest | grep -E '\.[ch]$' || true)
else
    files=$(git ls-files -- kernel ktest | grep -E '\.[ch]$' || true)
fi

if test -z "${files}"; then
    exit 0
fi

# shellcheck disable=SC2086
if "${check}"; then
    exec ${formatter} --dry-run -Werror ${files}
fi

# shellcheck disable=SC2086
exec ${formatter} -i ${files}

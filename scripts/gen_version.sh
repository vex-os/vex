#!/bin/sh
# argv[1]: version string
# argv[2]: machine string

build_date=$(LC_ALL=C date -u)
machine_string=${2}
git_revision=$(git rev-parse --short=8 HEAD || echo "00000000")
version=${1}

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

printf "#include <sys/version.h>\n"
printf "const char build_date[] = \"${build_date}\";\n"
printf "const char machine_string[] = \"${machine_string}\";\n"
printf "const char git_revision[] = \"${git_revision}\";\n"
printf "const char version[] = \"${version}\";\n"

#!/bin/sh

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

printf "#ifndef __INCLUDE_SYS_VERSION_H__\n"
printf "#define __INCLUDE_SYS_VERSION_H__\n"

printf "#define VERSION \"${1}\"\n"
printf "#define GIT_REV 0x${2}\n"

if [ "${2}" = "0" ]; then
    printf "#define SEMVER \"${1}\"\n"
else
    printf "#define SEMVER \"${1}+${2}\"\n"
fi

printf "#endif /* __INCLUDE_SYS_VERSION_H__ */\n"

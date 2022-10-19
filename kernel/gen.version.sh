#!/bin/bash

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

printf "#ifndef __INCLUDE_KANERU_VERSION_H__\n"
printf "#define __INCLUDE_KANERU_VERSION_H__\n"

printf "#define K_VERSION \"${1}\"\n"
printf "#define K_GIT_REV 0x${2}\n"

if [ "${2}" = "0" ]; then
    printf "#define K_SEMVER \"${1}\"\n"
else
    printf "#define K_SEMVER \"${1}+${2}\"\n"
fi

printf "#endif /* __INCLUDE_KANERU_VERSION_H__ */\n"

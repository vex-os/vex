#!/bin/sh
# [1]: version string

conf_version=${1}
conf_git_rev=$(git rev-parse --short=8 HEAD || echo 0)

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

printf "#ifndef _INCLUDE_CONF_VERSION_H__\n"
printf "#define _INCLUDE_CONF_VERSION_H__\n"

printf "#define CONF_VERSION \"${conf_version}\"\n"
printf "#define CONF_GIT_REV \"${conf_git_rev}\"\n"

if [ "${conf_git_rev}" = "0" ]
then
    printf "#define CONF_SEMVER \"${conf_version}\"\n"
else
    printf "#define CONF_SEMVER \"${conf_version}+${conf_git_rev}\"\n"
fi

printf "#endif /* _INCLUDE_CONF_VERSION_H__ */\n"

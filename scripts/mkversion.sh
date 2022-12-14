#!/bin/sh
# [1]: version string

# store values for future use
conf_version=${1}
conf_git_rev=$(git rev-parse --short=8 HEAD || echo 0)

# file header comment
printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

# include guard begin
printf "#ifndef _INCLUDE_CONF_VERSION_H__\n"
printf "#define _INCLUDE_CONF_VERSION_H__\n"

# separate semver parts
printf "#define CONF_VERSION \"${conf_version}\"\n"
printf "#define CONF_GIT_REV \"${conf_git_rev}\"\n"

# the full semver string
if [ "${conf_git_rev}" = "0" ]
then
    printf "#define CONF_SEMVER \"${conf_version}\"\n"
else
    printf "#define CONF_SEMVER \"${conf_version}+${conf_git_rev}\"\n"
fi

# include guard end
printf "#endif /* _INCLUDE_CONF_VERSION_H__ */\n"

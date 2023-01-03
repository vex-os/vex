/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <conf/version.h>
#include <kan/symbol.h>
#include <kan/version.h>

const char kernel_version[] = CONF_VERSION;
const char kernel_git_rev[] = CONF_GIT_REV;
const char kernel_semver[] = CONF_SEMVER;

EXPORT_SYMBOL(kernel_version);
EXPORT_SYMBOL(kernel_git_rev);
EXPORT_SYMBOL(kernel_semver);

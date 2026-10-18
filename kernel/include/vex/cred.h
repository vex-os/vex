#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

enum {
    CAP_DAC_OVERRIDE, // Bypass file permission checks
    CAP_MOUNT,
    CAP_SETUID,
    CAP_SETGID,
};

struct cred {
    uid_t uid;
    uid_t euid;
    gid_t gid;
    gid_t egid;
};

extern const struct cred sys_cred;

bool cred_capable(const struct cred* restrict cred, unsigned cap) __nodiscard;

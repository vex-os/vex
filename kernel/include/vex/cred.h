#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

enum {
    CAP_DAC_OVERRIDE, // Bypass file permission checks
    CAP_MOUNT,
    CAP_SETUID,
    CAP_SETGID,
};

struct credentials {
    uid_t uid;
    uid_t euid;
    uid_t suid;

    gid_t gid;
    gid_t egid;
    gid_t sgid;
};

extern const struct credentials sys_cred;

bool cred_capable(const struct credentials* restrict cred, unsigned cap) __nodiscard;

int cred_setresuid(struct credentials* restrict cred, uid_t ruid, uid_t euid, uid_t suid) __nodiscard;
int cred_setresgid(struct credentials* restrict cred, gid_t rgid, gid_t egid, gid_t sgid) __nodiscard;

#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define CAP_DAC_OVERRIDE UINT64_C(0x0000000000000001) // Bypass file permission checks
#define CAP_MOUNT        UINT64_C(0x0000000000000002)
#define CAP_SETUID       UINT64_C(0x0000000000000004)
#define CAP_SETGID       UINT64_C(0x0000000000000008)

struct credentials {
    unsigned refcount;

    uid_t uid;
    uid_t euid;
    uid_t suid;

    gid_t gid;
    gid_t egid;
    gid_t sgid;

    size_t ngroups;
    gid_t groups[];
};

extern struct credentials sys_cred;

struct credentials* cred_copy(const struct credentials* restrict cred) __nodiscard;
void cred_incref(struct credentials* restrict cred);
void cred_decref(struct credentials* restrict cred);

bool cred_capable(const struct credentials* restrict cred, uint64_t cap) __nodiscard;

int cred_setresuid(struct credentials* restrict cred, uid_t ruid, uid_t euid, uid_t suid) __nodiscard;
int cred_setresgid(struct credentials* restrict cred, gid_t rgid, gid_t egid, gid_t sgid) __nodiscard;

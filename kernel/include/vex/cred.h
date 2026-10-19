#pragma once

#include <vex/capability.h>
#include <vex/compiler.h>
#include <vex/types.h>

struct credentials {
    unsigned refcount;

    uid_t uid;
    uid_t euid;
    uid_t suid;

    gid_t gid;
    gid_t egid;
    gid_t sgid;

    uint64_t cap_effective;
    uint64_t cap_permitted;
    uint64_t cap_ambient;
    uint64_t cap_bounding;
    unsigned cap_flags;

    size_t ngroups;
    gid_t groups[];
};

struct file_caps {
    uint64_t permitted;
    uint64_t inheritable;
    bool effective;
};

extern struct credentials sys_cred;
extern size_t cred_ngroups_max;

struct credentials* cred_copy(const struct credentials* restrict cred) __nodiscard;
struct credentials* cred_copy_ids(const struct credentials* restrict cred, size_t ngroups) __nodiscard;
void cred_incref(struct credentials* restrict cred);
void cred_decref(struct credentials* restrict cred);

bool cred_capable(const struct credentials* restrict cred, unsigned cap) __nodiscard;
int cred_setcaps(struct credentials* restrict cred, const struct k_caps* restrict caps) __nodiscard;
int cred_exec_caps(struct credentials* restrict cred, bool privileged, const struct file_caps* restrict file) __nodiscard;
bool cred_exec_secure(const struct credentials* restrict cred) __nodiscard;
int cred_parse_file_caps(const void* restrict value, size_t size, struct file_caps* restrict out) __nodiscard;
bool cred_in_group(const struct credentials* restrict cred, gid_t gid) __nodiscard;

int cred_setresuid(struct credentials* restrict cred, uid_t ruid, uid_t euid, uid_t suid) __nodiscard;
int cred_setresgid(struct credentials* restrict cred, gid_t rgid, gid_t egid, gid_t sgid) __nodiscard;
int cred_sort_groups(struct credentials* restrict cred) __nodiscard;

#include <vex/assert.h>
#include <vex/cred.h>
#include <vex/errno.h>

#define ID_UNCHANGED (-1)

static_assert(sizeof(uid_t) == sizeof(gid_t));

const struct credentials sys_cred = { 0 };

static int check_id(int32_t wanted, int32_t r, int32_t e, int32_t s, bool capable)
{
    if(wanted < ID_UNCHANGED) {
        return -EINVAL;
    }

    if(capable || wanted == ID_UNCHANGED || wanted == r || wanted == e || wanted == s) {
        return 0;
    }

    return -EPERM;
}

static int check_res(int32_t new_r, int32_t new_e, int32_t new_s, int32_t r, int32_t e, int32_t s, bool capable)
{
    int retval = check_id(new_r, r, e, s, capable);

    if(retval == 0) {
        retval = check_id(new_e, r, e, s, capable);
    }

    if(retval == 0) {
        retval = check_id(new_s, r, e, s, capable);
    }

    return retval;
}

static void apply_id(int32_t* restrict id, int32_t wanted)
{
    if(wanted != ID_UNCHANGED) {
        *id = wanted;
    }
}

bool cred_capable(const struct credentials* restrict cred, unsigned cap)
{
    kassert(cred);

    // Every capability belongs to the superuser
    // until there is a per-process capability set
    return cred->euid == 0;
}

int cred_setresuid(struct credentials* restrict cred, uid_t ruid, uid_t euid, uid_t suid)
{
    kassert(cred);

    bool capable = cred_capable(cred, CAP_SETUID);
    int retval = check_res(ruid, euid, suid, cred->uid, cred->euid, cred->suid, capable);

    if(retval < 0) {
        return retval;
    }

    apply_id(&cred->uid, ruid);
    apply_id(&cred->euid, euid);
    apply_id(&cred->suid, suid);

    return 0;
}

int cred_setresgid(struct credentials* restrict cred, gid_t rgid, gid_t egid, gid_t sgid)
{
    kassert(cred);

    bool capable = cred_capable(cred, CAP_SETGID);
    int retval = check_res(rgid, egid, sgid, cred->gid, cred->egid, cred->sgid, capable);

    if(retval < 0) {
        return retval;
    }

    apply_id(&cred->gid, rgid);
    apply_id(&cred->egid, egid);
    apply_id(&cred->sgid, sgid);

    return 0;
}

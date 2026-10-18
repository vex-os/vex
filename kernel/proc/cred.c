#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/cred.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/slab.h>
#include <vex/stdlib.h>
#include <vex/string.h>

#define ID_UNCHANGED (-1)

static_assert(sizeof(uid_t) == sizeof(gid_t));

struct credentials sys_cred = {
    .refcount = 1,
    .cap_effective = CAP_FULL_SET,
    .cap_permitted = CAP_FULL_SET,
    .cap_bounding = CAP_FULL_SET,
};

size_t cred_ngroups_max = CONFIG_NGROUPS_MAX;

static __always_inline __nodiscard size_t cred_size(size_t ngroups)
{
    return sizeof(struct credentials) + ngroups * sizeof(gid_t);
}

static struct credentials* cred_alloc(size_t ngroups)
{
    size_t size = cred_size(ngroups);

    if(size <= SLAB_MAX_SIZE) {
        return slab_alloc(size);
    }

    return kmalloc(size);
}

static int compare_gids(const void* a, const void* b)
{
    gid_t gid_a = *(const gid_t*)a;
    gid_t gid_b = *(const gid_t*)b;

    if(gid_a < gid_b) {
        return -1;
    }

    if(gid_a > gid_b) {
        return 1;
    }

    return 0;
}

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

static void fixup_caps(struct credentials* restrict cred, uid_t old_r, uid_t old_e, uid_t old_s)
{
    bool was_root = old_r == 0 || old_e == 0 || old_s == 0;
    bool is_root = cred->uid == 0 || cred->euid == 0 || cred->suid == 0;

    if(was_root && !is_root && 0 == (cred->cap_flags & CAPS_KEEP)) {
        cred->cap_permitted = 0;
        cred->cap_ambient = 0;
    }

    if(old_e == 0 && cred->euid) {
        cred->cap_effective = 0;
    }

    if(old_e && cred->euid == 0) {
        cred->cap_effective = cred->cap_permitted;
    }
}

static void apply_id(int32_t* restrict id, int32_t wanted)
{
    if(wanted != ID_UNCHANGED) {
        *id = wanted;
    }
}

struct credentials* cred_copy(const struct credentials* restrict cred)
{
    kassert(cred);

    struct credentials* copy = cred_copy_ids(cred, cred->ngroups);

    if(copy) {
        memcpy(copy->groups, cred->groups, cred->ngroups * sizeof(gid_t));
    }

    return copy;
}

struct credentials* cred_copy_ids(const struct credentials* restrict cred, size_t ngroups)
{
    kassert(cred);

    struct credentials* copy = cred_alloc(ngroups);

    if(copy == nullptr) {
        return nullptr;
    }

    memcpy(copy, cred, sizeof(struct credentials));
    copy->refcount = 1;
    copy->ngroups = ngroups;

    return copy;
}

void cred_incref(struct credentials* restrict cred)
{
    kassert(cred);

    atomic_fetch_add(&cred->refcount, 1, ATOMIC_RELAXED);
}

void cred_decref(struct credentials* restrict cred)
{
    kassert(cred);

    if(1 == atomic_fetch_sub(&cred->refcount, 1, ATOMIC_ACQ_REL)) {
        kassert(cred != &sys_cred);

        if(cred_size(cred->ngroups) <= SLAB_MAX_SIZE) {
            slab_free(cred);
        }
        else {
            kfree(cred);
        }
    }
}

bool cred_capable(const struct credentials* restrict cred, unsigned cap)
{
    kassert(cred);
    kassert(cap < 64);

    return (bool)(cred->cap_effective & CAP_TO_MASK(cap));
}

int cred_setcaps(struct credentials* restrict cred, const struct k_caps* restrict caps)
{
    kassert(cred);
    kassert(caps);

    if((caps->flags & ~CAPS_KEEP) || caps->reserved) {
        return -EINVAL;
    }

    if((caps->permitted & ~cred->cap_permitted) || (caps->bounding & ~cred->cap_bounding)) {
        return -EPERM;
    }

    if((caps->effective & ~caps->permitted) || (caps->ambient & ~caps->permitted)) {
        return -EPERM;
    }

    cred->cap_effective = caps->effective;
    cred->cap_permitted = caps->permitted;
    cred->cap_ambient = caps->ambient;
    cred->cap_bounding = caps->bounding;
    cred->cap_flags = caps->flags;

    return 0;
}

// Called with the ids the program will run with already in place;
// a privileged program is one whose setuid or setgid bit took effect
void cred_exec_caps(struct credentials* restrict cred, bool privileged)
{
    kassert(cred);

    if(privileged) {
        cred->cap_ambient = 0;
    }

    if(cred->euid == 0) {
        cred->cap_permitted = cred->cap_bounding;
    }
    else {
        cred->cap_permitted = cred->cap_ambient & cred->cap_bounding;
    }

    cred->cap_ambient &= cred->cap_permitted;
    cred->cap_effective = cred->cap_permitted;
    cred->cap_flags = 0;
}

bool cred_in_group(const struct credentials* restrict cred, gid_t gid)
{
    kassert(cred);

    if(cred->egid == gid) {
        return true;
    }

    size_t low = 0;
    size_t high = cred->ngroups;

    while(low < high) {
        size_t mid = low + (high - low) / 2;

        if(cred->groups[mid] == gid) {
            return true;
        }

        if(cred->groups[mid] < gid) {
            low = mid + 1;
        }
        else {
            high = mid;
        }
    }

    return false;
}

int cred_setresuid(struct credentials* restrict cred, uid_t ruid, uid_t euid, uid_t suid)
{
    kassert(cred);

    bool capable = cred_capable(cred, CAP_SETUID);
    int retval = check_res(ruid, euid, suid, cred->uid, cred->euid, cred->suid, capable);

    if(retval < 0) {
        return retval;
    }

    uid_t old_r = cred->uid;
    uid_t old_e = cred->euid;
    uid_t old_s = cred->suid;

    apply_id(&cred->uid, ruid);
    apply_id(&cred->euid, euid);
    apply_id(&cred->suid, suid);

    fixup_caps(cred, old_r, old_e, old_s);

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

int cred_sort_groups(struct credentials* restrict cred)
{
    kassert(cred);

    for(size_t i = 0; i < cred->ngroups; ++i) {
        if(cred->groups[i] < 0) {
            return -EINVAL;
        }
    }

    sort(cred->groups, cred->ngroups, sizeof(gid_t), &compare_gids);

    return 0;
}

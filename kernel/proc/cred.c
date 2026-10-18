#include <vex/assert.h>
#include <vex/cred.h>

const struct cred sys_cred = { 0 };

bool cred_capable(const struct cred* restrict cred, unsigned cap)
{
    kassert(cred);

    // Every capability belongs to the superuser
    // until there is a per-process capability set
    return cred->euid == 0;
}

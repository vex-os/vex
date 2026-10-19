#pragma once

#include <usr/vex/capability.h>

#define CAP_CHOWN        __k_CAP_CHOWN
#define CAP_DAC_OVERRIDE __k_CAP_DAC_OVERRIDE
#define CAP_FOWNER       __k_CAP_FOWNER
#define CAP_FSETID       __k_CAP_FSETID
#define CAP_SETGID       __k_CAP_SETGID
#define CAP_SETUID       __k_CAP_SETUID
#define CAP_SYS_ADMIN    __k_CAP_SYS_ADMIN
#define CAP_SETFCAP      __k_CAP_SETFCAP
#define CAP_MOUNT        __k_CAP_MOUNT

#define CAP_TO_MASK(cap) __k_CAP_TO_MASK(cap)
#define CAP_FULL_SET     UINT64_MAX

#define CAPS_KEEP __k_CAPS_KEEP

#define k_caps __k_caps

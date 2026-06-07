#include <sys/version.h>

#ifndef UTSNAME_SYSNAME
#define UTSNAME_SYSNAME "Unknown"
#endif

#ifndef UTSNAME_RELEASE
#define UTSNAME_RELEASE "0.0.0"
#endif

#ifndef UTSNAME_VERSION
#define UTSNAME_VERSION "unknown"
#endif

#ifndef UTSNAME_MACHINE
#define UTSNAME_MACHINE "unknown"
#endif

const char k_sysname[] = UTSNAME_SYSNAME;
const char k_release[] = UTSNAME_RELEASE;
const char k_version[] = UTSNAME_VERSION;
const char k_machine[] = UTSNAME_MACHINE;

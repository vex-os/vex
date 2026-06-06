#ifndef CEA2ABEF_CBCA_4217_812E_FC0FAC3967E9
#define CEA2ABEF_CBCA_4217_812E_FC0FAC3967E9
#pragma once

// These values can also be accessed as preprocessor
// macros starting with UTSNAME prefix (eg. UTSNAME_SYSNAME)
// but it seems like a good idea to have them as globals
// as well - it's more convenient to copy them to userspace this way

extern const char k_sysname[]; ///< system name
extern const char k_release[]; ///< version number
extern const char k_version[]; ///< build information
extern const char k_machine[]; ///< hardware identifier

#endif /* CEA2ABEF_CBCA_4217_812E_FC0FAC3967E9 */

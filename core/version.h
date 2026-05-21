// SPDX-License-Identifier: BSD-3-Clause
#ifndef BBBC19F1_0D86_49FC_9D9A_83D964191B5A
#define BBBC19F1_0D86_49FC_9D9A_83D964191B5A
#pragma once

// These values can also be accessed as preprocessor
// macros starting with UTSNAME prefix (eg. UTSNAME_SYSNAME)
// but it seems like a good idea to have them as globals
// as well - it's more convenient to copy them to userspace this way

extern const char k_sysname[]; ///< system name
extern const char k_release[]; ///< version number
extern const char k_version[]; ///< build information
extern const char k_machine[]; ///< hardware identifier

#endif /* BBBC19F1_0D86_49FC_9D9A_83D964191B5A */

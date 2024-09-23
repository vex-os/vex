/* SPDX-License-Identifier: Zlib */
#include <string.h>

char *strerror(int errnum)
{
    static char buffer[128] = { 0 };
    strerror_r(errnum, buffer, sizeof(buffer));
    return buffer;
}

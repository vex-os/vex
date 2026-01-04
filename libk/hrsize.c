#include <vex/hrsize.h>
#include <vex/sprintf.h>
#include <vex/string.h>

int hrsize_bin(char* restrict buffer, size_t sz, size_t value)
{
    static const char* suffixes[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB" };
    static const size_t num_suffixes = sizeof(suffixes) / sizeof(suffixes[0]);

    size_t i;
    size_t index = 0U;

    for(i = 0U; i < num_suffixes; ++i) {
        if(value >= 1600U) {
            value >>= 10U;
            index += 1U;
        }
    }

    memset(buffer, 0, sz);

    return snprintf(buffer, sz, "%zu %s", value, suffixes[index]);
}

int hrsize_dec(char* restrict buffer, size_t sz, size_t value)
{
    static const char* suffixes[] = { "B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB" };
    static const size_t num_suffixes = sizeof(suffixes) / sizeof(suffixes[0]);

    size_t i;
    size_t index = 0U;

    for(i = 0U; i < num_suffixes; ++i) {
        if(value >= 1600U) {
            value /= 1000U;
            index += 1U;
        }
    }

    memset(buffer, 0, sz);

    return snprintf(buffer, sz, "%zu %s", value, suffixes[index]);
}

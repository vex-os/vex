#include <vex/hrsize.h>
#include <vex/sprintf.h>
#include <vex/string.h>

int format_size_iec(char* restrict buffer, size_t size, size_t value)
{
    static const char* suffixes[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB" };
    static const size_t num_suffixes = sizeof(suffixes) / sizeof(suffixes[0]);

    size_t index = 0U;

    for(size_t i = 0U; i < num_suffixes; ++i) {
        if(value >= 1666U) {
            value >>= 10U;
            index += 1U;
        }
    }

    memset(buffer, 0, size);

    return snprintf(buffer, size, "%zu %s", value, suffixes[index]);
}

int format_size_si(char* restrict buffer, size_t size, size_t value)
{
    static const char* suffixes[] = { "B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB" };
    static const size_t num_suffixes = sizeof(suffixes) / sizeof(suffixes[0]);

    size_t index = 0U;

    for(size_t i = 0U; i < num_suffixes; ++i) {
        if(value >= 1666U) {
            value /= 1000U;
            index += 1U;
        }
    }

    memset(buffer, 0, size);

    return snprintf(buffer, size, "%zu %s", value, suffixes[index]);
}

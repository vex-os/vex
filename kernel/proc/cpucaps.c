#include <vex/assert.h>
#include <vex/cpucaps.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/sprintf.h>
#include <vex/string.h>

#define NAMES_SIZE 256

static cpucaps_t system_caps;
static bool committed = false;

static void format_names(char* restrict buffer, const cpucaps_t* restrict caps)
{
    size_t length = 0;

    buffer[0] = 0x00;

    for(size_t i = 0; i < NUM_CPU_FEATURES && length < NAMES_SIZE; ++i) {
        if(cpucaps_has(caps, i)) {
            int written;

            if(length == 0) {
                written = snprintf(&buffer[length], NAMES_SIZE - length, "%s", arch_cpu_features[i].name);
            }
            else {
                written = snprintf(&buffer[length], NAMES_SIZE - length, " %s", arch_cpu_features[i].name);
            }

            if(written < 0) {
                break;
            }

            length += (size_t)(written);
        }
    }
}

static bool find_missing(const cpucaps_t* restrict caps, cpucaps_t* restrict missing)
{
    bool found = false;

    memset(missing, 0, sizeof(cpucaps_t));

    for(size_t i = 0; i < NUM_CPU_FEATURES; ++i) {
        if(arch_cpu_features[i].scope == CPU_SCOPE_REQUIRED && !cpucaps_has(caps, i)) {
            cpucaps_set(missing, i);
            found = true;
        }
    }

    return found;
}

bool cpu_has(cpu_feature_t feature)
{
    kassert(committed);
    kassert(arch_cpu_features[feature].scope != CPU_SCOPE_LOCAL);

    return cpucaps_has(&system_caps, feature);
}

void cpucaps_init(const cpucaps_t* restrict boot_caps)
{
    char names[NAMES_SIZE];
    cpucaps_t missing;

    for(size_t i = 0; i < NUM_CPU_FEATURES; ++i) {
        if(arch_cpu_features[i].name == nullptr) {
            panic("cpucaps: feature %zu has no description", i);
        }
    }

    if(find_missing(boot_caps, &missing)) {
        format_names(names, &missing);
        panic("cpucaps: the boot hart lacks %s", names);
    }

    system_caps = *boot_caps;

    for(size_t i = 0; i < NUM_CPU_FEATURES; ++i) {
        if(arch_cpu_features[i].scope == CPU_SCOPE_LOCAL) {
            bitmap_clear(system_caps.features, i);
        }
    }
}

bool cpucaps_merge(const cpucaps_t* restrict caps, unsigned short index)
{
    char names[NAMES_SIZE];
    cpucaps_t missing;

    kassert(!committed);

    if(find_missing(caps, &missing)) {
        format_names(names, &missing);
        pr_critical("cpucaps: hart %hu lacks %s, leaving it offline", index, names);
        return false;
    }

    cpucaps_t lost;
    bool lost_any = false;

    for(size_t i = 0; i < CPUCAPS_CHUNKS; ++i) {
        lost.features[i] = system_caps.features[i] & ~caps->features[i];
        system_caps.features[i] &= caps->features[i];
        lost_any = lost_any || lost.features[i];
    }

    if(lost_any) {
        format_names(names, &lost);
        pr_warning("cpucaps: hart %hu lacks %s, disabling it on every hart", index, names);
    }

    return true;
}

void cpucaps_commit(void)
{
    committed = true;
}

static void report_cpucaps(void)
{
    char names[NAMES_SIZE];

    format_names(names, &system_caps);
    pr_verbose("cpucaps: %s", names);
}

initcall_bsp_multicore(report_cpucaps, INIT_FIRST);

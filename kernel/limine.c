#include <vex/limine.h>

static volatile __used __section(".limine_requests") LIMINE_BASE_REVISION(4);
static volatile __used __section(".limine_requests_start") LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") LIMINE_REQUESTS_END_MARKER;

#include <sys/assert.h>
#include <sys/crc32_iso3309.h>
#include <sys/initcall.h>

struct crc32_table crc32_iso3309_table;

define_initcall(crc32_iso3309, INITCALL_STAGE_BSP_EARLY, INITCALL_ORDER_MIDDLE)
{
    crc32_generate_rfi(&crc32_iso3309_table, UINT32_C(0xEDB88320), UINT32_C(0xFFFFFFFF));
    kassert(UINT32_C(0xCBF43926) == crc32_iso3309_get("123456789", 9));
}

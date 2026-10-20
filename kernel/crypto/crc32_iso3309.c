#include <vex/assert.h>
#include <vex/crc32_iso3309.h>
#include <vex/initcall.h>

crc32_table_t crc32_iso3309_table;

static void init_crc32_iso3309(void)
{
    crc32_table_init_refin(&crc32_iso3309_table, UINT32_C(0xEDB88320), UINT32_C(0xFFFFFFFF));
    kassert(UINT32_C(0xCBF43926) == crc32_iso3309_compute("123456789", 9));
}

initcall_bsp_early(init_crc32_iso3309, INIT_MIDDLE);

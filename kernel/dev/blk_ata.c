#include <vex/bitmap.h>
#include <vex/blk_ata.h>
#include <vex/ctype.h>

void ata_identify_string(const uint16_t* restrict field, size_t field_nw, char* restrict string, size_t size)
{
    size_t pos = 0;

    for(size_t i = 0; i < field_nw && pos + 2 <= size - 1; ++i) {
        uint16_t word = le16toh(field[i]);
        string[pos++] = word >> 8;
        string[pos++] = word & 0xFF;
    }

    string[pos] = 0x00;

    while(pos > 0 && isspace(string[pos - 1])) {
        string[--pos] = 0x00;
    }
}

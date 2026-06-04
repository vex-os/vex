#include <dev/ata.h>
#include <lib/ctype.h>

void ata_identify_string(const uint16_t* restrict field, size_t field_nw, char* restrict string, size_t sz)
{
    size_t pos = 0;

    for(size_t i = 0; i < field_nw && pos + 2 <= sz - 1; ++i) {
        uint16_t word = LE16_to_host(field[i]);
        string[pos++] = (char)(word >> 8);
        string[pos++] = (char)(word & 0xFF);
    }

    string[pos] = 0x00;

    while(pos > 0 && isspace(string[pos - 1])) {
        string[--pos] = 0x00;
    }
}

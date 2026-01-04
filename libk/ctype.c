#include <vex/ctype.h>

#define CT_CT (1 << 0)
#define CT_GR (1 << 1)
#define CT_SP (1 << 2)
#define CT_BL (1 << 3)
#define CT_UC (1 << 4)
#define CT_LC (1 << 5)
#define CT_DD (1 << 6)
#define CT_XD (1 << 7)
#define CT_PT (1 << 8)
#define CT_WS (1 << 9)

#define CTYPE_SIZE 256

struct ctype_ent {
    int lower;
    int upper;
    int flags;
};

static struct ctype_ent ctype[CTYPE_SIZE] = {
    /* NUL  */ { .lower = 0x00, .upper = 0x00, .flags = CT_CT },
    /* SOH  */ { .lower = 0x01, .upper = 0x01, .flags = CT_CT },
    /* STX  */ { .lower = 0x02, .upper = 0x02, .flags = CT_CT },
    /* ETX  */ { .lower = 0x03, .upper = 0x03, .flags = CT_CT },
    /* EOT  */ { .lower = 0x04, .upper = 0x04, .flags = CT_CT },
    /* ENQ  */ { .lower = 0x05, .upper = 0x05, .flags = CT_CT },
    /* ACK  */ { .lower = 0x06, .upper = 0x06, .flags = CT_CT },
    /* BEL  */ { .lower = 0x07, .upper = 0x07, .flags = CT_CT },
    /* BS   */ { .lower = 0x08, .upper = 0x08, .flags = CT_CT },
    /* HT   */ { .lower = 0x09, .upper = 0x09, .flags = CT_CT | CT_SP | CT_BL },
    /* LF   */ { .lower = 0x0A, .upper = 0x0A, .flags = CT_CT | CT_SP },
    /* VT   */ { .lower = 0x0B, .upper = 0x0B, .flags = CT_CT | CT_SP },
    /* FF   */ { .lower = 0x0C, .upper = 0x0C, .flags = CT_CT | CT_SP },
    /* CR   */ { .lower = 0x0D, .upper = 0x0D, .flags = CT_CT | CT_SP },
    /* SO   */ { .lower = 0x0E, .upper = 0x0E, .flags = CT_CT },
    /* SI   */ { .lower = 0x0F, .upper = 0x0F, .flags = CT_CT },
    /* DLE  */ { .lower = 0x10, .upper = 0x10, .flags = CT_CT },
    /* DC1  */ { .lower = 0x11, .upper = 0x11, .flags = CT_CT },
    /* DC2  */ { .lower = 0x12, .upper = 0x12, .flags = CT_CT },
    /* DC3  */ { .lower = 0x13, .upper = 0x13, .flags = CT_CT },
    /* DC4  */ { .lower = 0x14, .upper = 0x14, .flags = CT_CT },
    /* NAK  */ { .lower = 0x15, .upper = 0x15, .flags = CT_CT },
    /* SYN  */ { .lower = 0x16, .upper = 0x16, .flags = CT_CT },
    /* ETB  */ { .lower = 0x17, .upper = 0x17, .flags = CT_CT },
    /* CAN  */ { .lower = 0x18, .upper = 0x18, .flags = CT_CT },
    /* EM   */ { .lower = 0x19, .upper = 0x19, .flags = CT_CT },
    /* SUB  */ { .lower = 0x1A, .upper = 0x1A, .flags = CT_CT },
    /* ESC  */ { .lower = 0x1B, .upper = 0x1B, .flags = CT_CT },
    /* FS   */ { .lower = 0x1C, .upper = 0x1C, .flags = CT_CT },
    /* GS   */ { .lower = 0x1D, .upper = 0x1D, .flags = CT_CT },
    /* RS   */ { .lower = 0x1E, .upper = 0x1E, .flags = CT_CT },
    /* US   */ { .lower = 0x1F, .upper = 0x1F, .flags = CT_CT },
    /* SP   */ { .lower = 0x20, .upper = 0x20, .flags = CT_SP | CT_BL | CT_WS },
    /* !    */ { .lower = 0x21, .upper = 0x21, .flags = CT_GR | CT_PT },
    /* "    */ { .lower = 0x22, .upper = 0x22, .flags = CT_GR | CT_PT },
    /* #    */ { .lower = 0x23, .upper = 0x23, .flags = CT_GR | CT_PT },
    /* $    */ { .lower = 0x24, .upper = 0x24, .flags = CT_GR | CT_PT },
    /* %    */ { .lower = 0x25, .upper = 0x25, .flags = CT_GR | CT_PT },
    /* &    */ { .lower = 0x26, .upper = 0x26, .flags = CT_GR | CT_PT },
    /* '    */ { .lower = 0x27, .upper = 0x27, .flags = CT_GR | CT_PT },
    /* (    */ { .lower = 0x28, .upper = 0x28, .flags = CT_GR | CT_PT },
    /* )    */ { .lower = 0x29, .upper = 0x29, .flags = CT_GR | CT_PT },
    /* *    */ { .lower = 0x2A, .upper = 0x2A, .flags = CT_GR | CT_PT },
    /* +    */ { .lower = 0x2B, .upper = 0x2B, .flags = CT_GR | CT_PT },
    /* ,    */ { .lower = 0x2C, .upper = 0x2C, .flags = CT_GR | CT_PT },
    /* -    */ { .lower = 0x2D, .upper = 0x2D, .flags = CT_GR | CT_PT },
    /* .    */ { .lower = 0x2E, .upper = 0x2E, .flags = CT_GR | CT_PT },
    /* /    */ { .lower = 0x2F, .upper = 0x2F, .flags = CT_GR | CT_PT },
    /* 0    */ { .lower = 0x30, .upper = 0x30, .flags = CT_GR | CT_DD | CT_XD },
    /* 1    */ { .lower = 0x31, .upper = 0x31, .flags = CT_GR | CT_DD | CT_XD },
    /* 2    */ { .lower = 0x32, .upper = 0x32, .flags = CT_GR | CT_DD | CT_XD },
    /* 3    */ { .lower = 0x33, .upper = 0x33, .flags = CT_GR | CT_DD | CT_XD },
    /* 4    */ { .lower = 0x34, .upper = 0x34, .flags = CT_GR | CT_DD | CT_XD },
    /* 5    */ { .lower = 0x35, .upper = 0x35, .flags = CT_GR | CT_DD | CT_XD },
    /* 6    */ { .lower = 0x36, .upper = 0x36, .flags = CT_GR | CT_DD | CT_XD },
    /* 7    */ { .lower = 0x37, .upper = 0x37, .flags = CT_GR | CT_DD | CT_XD },
    /* 8    */ { .lower = 0x38, .upper = 0x38, .flags = CT_GR | CT_DD | CT_XD },
    /* 9    */ { .lower = 0x39, .upper = 0x39, .flags = CT_GR | CT_DD | CT_XD },
    /* :    */ { .lower = 0x3A, .upper = 0x3A, .flags = CT_GR | CT_PT },
    /* ;    */ { .lower = 0x3B, .upper = 0x3B, .flags = CT_GR | CT_PT },
    /* <    */ { .lower = 0x3C, .upper = 0x3C, .flags = CT_GR | CT_PT },
    /* =    */ { .lower = 0x3D, .upper = 0x3D, .flags = CT_GR | CT_PT },
    /* >    */ { .lower = 0x3E, .upper = 0x3E, .flags = CT_GR | CT_PT },
    /* ?    */ { .lower = 0x3F, .upper = 0x3F, .flags = CT_GR | CT_PT },
    /* @    */ { .lower = 0x40, .upper = 0x40, .flags = CT_GR | CT_PT },
    /* A    */ { .lower = 0x61, .upper = 0x41, .flags = CT_GR | CT_UC | CT_XD },
    /* B    */ { .lower = 0x62, .upper = 0x42, .flags = CT_GR | CT_UC | CT_XD },
    /* C    */ { .lower = 0x63, .upper = 0x43, .flags = CT_GR | CT_UC | CT_XD },
    /* D    */ { .lower = 0x64, .upper = 0x44, .flags = CT_GR | CT_UC | CT_XD },
    /* E    */ { .lower = 0x65, .upper = 0x45, .flags = CT_GR | CT_UC | CT_XD },
    /* F    */ { .lower = 0x66, .upper = 0x46, .flags = CT_GR | CT_UC | CT_XD },
    /* G    */ { .lower = 0x67, .upper = 0x47, .flags = CT_GR | CT_UC },
    /* H    */ { .lower = 0x68, .upper = 0x48, .flags = CT_GR | CT_UC },
    /* I    */ { .lower = 0x69, .upper = 0x49, .flags = CT_GR | CT_UC },
    /* J    */ { .lower = 0x6A, .upper = 0x4A, .flags = CT_GR | CT_UC },
    /* K    */ { .lower = 0x6B, .upper = 0x4B, .flags = CT_GR | CT_UC },
    /* L    */ { .lower = 0x6C, .upper = 0x4C, .flags = CT_GR | CT_UC },
    /* M    */ { .lower = 0x6D, .upper = 0x4D, .flags = CT_GR | CT_UC },
    /* N    */ { .lower = 0x6E, .upper = 0x4E, .flags = CT_GR | CT_UC },
    /* O    */ { .lower = 0x6F, .upper = 0x4F, .flags = CT_GR | CT_UC },
    /* P    */ { .lower = 0x70, .upper = 0x50, .flags = CT_GR | CT_UC },
    /* Q    */ { .lower = 0x71, .upper = 0x51, .flags = CT_GR | CT_UC },
    /* R    */ { .lower = 0x72, .upper = 0x52, .flags = CT_GR | CT_UC },
    /* S    */ { .lower = 0x73, .upper = 0x53, .flags = CT_GR | CT_UC },
    /* T    */ { .lower = 0x74, .upper = 0x54, .flags = CT_GR | CT_UC },
    /* U    */ { .lower = 0x75, .upper = 0x55, .flags = CT_GR | CT_UC },
    /* V    */ { .lower = 0x76, .upper = 0x56, .flags = CT_GR | CT_UC },
    /* W    */ { .lower = 0x77, .upper = 0x57, .flags = CT_GR | CT_UC },
    /* X    */ { .lower = 0x78, .upper = 0x58, .flags = CT_GR | CT_UC },
    /* Y    */ { .lower = 0x79, .upper = 0x59, .flags = CT_GR | CT_UC },
    /* Z    */ { .lower = 0x7A, .upper = 0x5A, .flags = CT_GR | CT_UC },
    /* [    */ { .lower = 0x5B, .upper = 0x5B, .flags = CT_GR | CT_PT },
    /* \    */ { .lower = 0x5C, .upper = 0x5C, .flags = CT_GR | CT_PT },
    /* ]    */ { .lower = 0x5D, .upper = 0x5D, .flags = CT_GR | CT_PT },
    /* ^    */ { .lower = 0x5E, .upper = 0x5E, .flags = CT_GR | CT_PT },
    /* _    */ { .lower = 0x5F, .upper = 0x5F, .flags = CT_GR | CT_PT },
    /* `    */ { .lower = 0x60, .upper = 0x60, .flags = CT_GR | CT_PT },
    /* a    */ { .lower = 0x61, .upper = 0x41, .flags = CT_GR | CT_LC | CT_XD },
    /* b    */ { .lower = 0x62, .upper = 0x42, .flags = CT_GR | CT_LC | CT_XD },
    /* c    */ { .lower = 0x63, .upper = 0x43, .flags = CT_GR | CT_LC | CT_XD },
    /* d    */ { .lower = 0x64, .upper = 0x44, .flags = CT_GR | CT_LC | CT_XD },
    /* e    */ { .lower = 0x65, .upper = 0x45, .flags = CT_GR | CT_LC | CT_XD },
    /* f    */ { .lower = 0x66, .upper = 0x46, .flags = CT_GR | CT_LC | CT_XD },
    /* g    */ { .lower = 0x67, .upper = 0x47, .flags = CT_GR | CT_LC },
    /* h    */ { .lower = 0x68, .upper = 0x48, .flags = CT_GR | CT_LC },
    /* i    */ { .lower = 0x69, .upper = 0x49, .flags = CT_GR | CT_LC },
    /* j    */ { .lower = 0x6A, .upper = 0x4A, .flags = CT_GR | CT_LC },
    /* k    */ { .lower = 0x6B, .upper = 0x4B, .flags = CT_GR | CT_LC },
    /* l    */ { .lower = 0x6C, .upper = 0x4C, .flags = CT_GR | CT_LC },
    /* m    */ { .lower = 0x6D, .upper = 0x4D, .flags = CT_GR | CT_LC },
    /* n    */ { .lower = 0x6E, .upper = 0x4E, .flags = CT_GR | CT_LC },
    /* o    */ { .lower = 0x6F, .upper = 0x4F, .flags = CT_GR | CT_LC },
    /* p    */ { .lower = 0x70, .upper = 0x50, .flags = CT_GR | CT_LC },
    /* q    */ { .lower = 0x71, .upper = 0x51, .flags = CT_GR | CT_LC },
    /* r    */ { .lower = 0x72, .upper = 0x52, .flags = CT_GR | CT_LC },
    /* s    */ { .lower = 0x73, .upper = 0x53, .flags = CT_GR | CT_LC },
    /* t    */ { .lower = 0x74, .upper = 0x54, .flags = CT_GR | CT_LC },
    /* u    */ { .lower = 0x75, .upper = 0x55, .flags = CT_GR | CT_LC },
    /* v    */ { .lower = 0x76, .upper = 0x56, .flags = CT_GR | CT_LC },
    /* w    */ { .lower = 0x77, .upper = 0x57, .flags = CT_GR | CT_LC },
    /* x    */ { .lower = 0x78, .upper = 0x58, .flags = CT_GR | CT_LC },
    /* y    */ { .lower = 0x79, .upper = 0x59, .flags = CT_GR | CT_LC },
    /* z    */ { .lower = 0x7A, .upper = 0x5A, .flags = CT_GR | CT_LC },
    /* {    */ { .lower = 0x7B, .upper = 0x7B, .flags = CT_GR | CT_PT },
    /* |    */ { .lower = 0x7C, .upper = 0x7C, .flags = CT_GR | CT_PT },
    /* }    */ { .lower = 0x7D, .upper = 0x7D, .flags = CT_GR | CT_PT },
    /* ~    */ { .lower = 0x7E, .upper = 0x7E, .flags = CT_GR | CT_PT },
    /* DEL  */ { .lower = 0x7F, .upper = 0x7F, .flags = CT_CT },
    /*      */ { .lower = 0x80, .upper = 0x80, .flags = 0 },
    /*      */ { .lower = 0x81, .upper = 0x81, .flags = 0 },
    /*      */ { .lower = 0x82, .upper = 0x82, .flags = 0 },
    /*      */ { .lower = 0x83, .upper = 0x83, .flags = 0 },
    /*      */ { .lower = 0x84, .upper = 0x84, .flags = 0 },
    /*      */ { .lower = 0x85, .upper = 0x85, .flags = 0 },
    /*      */ { .lower = 0x86, .upper = 0x86, .flags = 0 },
    /*      */ { .lower = 0x87, .upper = 0x87, .flags = 0 },
    /*      */ { .lower = 0x88, .upper = 0x88, .flags = 0 },
    /*      */ { .lower = 0x89, .upper = 0x89, .flags = 0 },
    /*      */ { .lower = 0x8A, .upper = 0x8A, .flags = 0 },
    /*      */ { .lower = 0x8B, .upper = 0x8B, .flags = 0 },
    /*      */ { .lower = 0x8C, .upper = 0x8C, .flags = 0 },
    /*      */ { .lower = 0x8D, .upper = 0x8D, .flags = 0 },
    /*      */ { .lower = 0x8E, .upper = 0x8E, .flags = 0 },
    /*      */ { .lower = 0x8F, .upper = 0x8F, .flags = 0 },
    /*      */ { .lower = 0x90, .upper = 0x90, .flags = 0 },
    /*      */ { .lower = 0x91, .upper = 0x91, .flags = 0 },
    /*      */ { .lower = 0x92, .upper = 0x92, .flags = 0 },
    /*      */ { .lower = 0x93, .upper = 0x93, .flags = 0 },
    /*      */ { .lower = 0x94, .upper = 0x94, .flags = 0 },
    /*      */ { .lower = 0x95, .upper = 0x95, .flags = 0 },
    /*      */ { .lower = 0x96, .upper = 0x96, .flags = 0 },
    /*      */ { .lower = 0x97, .upper = 0x97, .flags = 0 },
    /*      */ { .lower = 0x98, .upper = 0x98, .flags = 0 },
    /*      */ { .lower = 0x99, .upper = 0x99, .flags = 0 },
    /*      */ { .lower = 0x9A, .upper = 0x9A, .flags = 0 },
    /*      */ { .lower = 0x9B, .upper = 0x9B, .flags = 0 },
    /*      */ { .lower = 0x9C, .upper = 0x9C, .flags = 0 },
    /*      */ { .lower = 0x9D, .upper = 0x9D, .flags = 0 },
    /*      */ { .lower = 0x9E, .upper = 0x9E, .flags = 0 },
    /*      */ { .lower = 0x9F, .upper = 0x9F, .flags = 0 },
    /*      */ { .lower = 0xA0, .upper = 0xA0, .flags = 0 },
    /*      */ { .lower = 0xA1, .upper = 0xA1, .flags = 0 },
    /*      */ { .lower = 0xA2, .upper = 0xA2, .flags = 0 },
    /*      */ { .lower = 0xA3, .upper = 0xA3, .flags = 0 },
    /*      */ { .lower = 0xA4, .upper = 0xA4, .flags = 0 },
    /*      */ { .lower = 0xA5, .upper = 0xA5, .flags = 0 },
    /*      */ { .lower = 0xA6, .upper = 0xA6, .flags = 0 },
    /*      */ { .lower = 0xA7, .upper = 0xA7, .flags = 0 },
    /*      */ { .lower = 0xA8, .upper = 0xA8, .flags = 0 },
    /*      */ { .lower = 0xA9, .upper = 0xA9, .flags = 0 },
    /*      */ { .lower = 0xAA, .upper = 0xAA, .flags = 0 },
    /*      */ { .lower = 0xAB, .upper = 0xAB, .flags = 0 },
    /*      */ { .lower = 0xAC, .upper = 0xAC, .flags = 0 },
    /*      */ { .lower = 0xAD, .upper = 0xAD, .flags = 0 },
    /*      */ { .lower = 0xAE, .upper = 0xAE, .flags = 0 },
    /*      */ { .lower = 0xAF, .upper = 0xAF, .flags = 0 },
    /*      */ { .lower = 0xB0, .upper = 0xB0, .flags = 0 },
    /*      */ { .lower = 0xB1, .upper = 0xB1, .flags = 0 },
    /*      */ { .lower = 0xB2, .upper = 0xB2, .flags = 0 },
    /*      */ { .lower = 0xB3, .upper = 0xB3, .flags = 0 },
    /*      */ { .lower = 0xB4, .upper = 0xB4, .flags = 0 },
    /*      */ { .lower = 0xB5, .upper = 0xB5, .flags = 0 },
    /*      */ { .lower = 0xB6, .upper = 0xB6, .flags = 0 },
    /*      */ { .lower = 0xB7, .upper = 0xB7, .flags = 0 },
    /*      */ { .lower = 0xB8, .upper = 0xB8, .flags = 0 },
    /*      */ { .lower = 0xB9, .upper = 0xB9, .flags = 0 },
    /*      */ { .lower = 0xBA, .upper = 0xBA, .flags = 0 },
    /*      */ { .lower = 0xBB, .upper = 0xBB, .flags = 0 },
    /*      */ { .lower = 0xBC, .upper = 0xBC, .flags = 0 },
    /*      */ { .lower = 0xBD, .upper = 0xBD, .flags = 0 },
    /*      */ { .lower = 0xBE, .upper = 0xBE, .flags = 0 },
    /*      */ { .lower = 0xBF, .upper = 0xBF, .flags = 0 },
    /*      */ { .lower = 0xC0, .upper = 0xC0, .flags = 0 },
    /*      */ { .lower = 0xC1, .upper = 0xC1, .flags = 0 },
    /*      */ { .lower = 0xC2, .upper = 0xC2, .flags = 0 },
    /*      */ { .lower = 0xC3, .upper = 0xC3, .flags = 0 },
    /*      */ { .lower = 0xC4, .upper = 0xC4, .flags = 0 },
    /*      */ { .lower = 0xC5, .upper = 0xC5, .flags = 0 },
    /*      */ { .lower = 0xC6, .upper = 0xC6, .flags = 0 },
    /*      */ { .lower = 0xC7, .upper = 0xC7, .flags = 0 },
    /*      */ { .lower = 0xC8, .upper = 0xC8, .flags = 0 },
    /*      */ { .lower = 0xC9, .upper = 0xC9, .flags = 0 },
    /*      */ { .lower = 0xCA, .upper = 0xCA, .flags = 0 },
    /*      */ { .lower = 0xCB, .upper = 0xCB, .flags = 0 },
    /*      */ { .lower = 0xCC, .upper = 0xCC, .flags = 0 },
    /*      */ { .lower = 0xCD, .upper = 0xCD, .flags = 0 },
    /*      */ { .lower = 0xCE, .upper = 0xCE, .flags = 0 },
    /*      */ { .lower = 0xCF, .upper = 0xCF, .flags = 0 },
    /*      */ { .lower = 0xD0, .upper = 0xD0, .flags = 0 },
    /*      */ { .lower = 0xD1, .upper = 0xD1, .flags = 0 },
    /*      */ { .lower = 0xD2, .upper = 0xD2, .flags = 0 },
    /*      */ { .lower = 0xD3, .upper = 0xD3, .flags = 0 },
    /*      */ { .lower = 0xD4, .upper = 0xD4, .flags = 0 },
    /*      */ { .lower = 0xD5, .upper = 0xD5, .flags = 0 },
    /*      */ { .lower = 0xD6, .upper = 0xD6, .flags = 0 },
    /*      */ { .lower = 0xD7, .upper = 0xD7, .flags = 0 },
    /*      */ { .lower = 0xD8, .upper = 0xD8, .flags = 0 },
};

int isalnum(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_UC | CT_LC | CT_DD);
}

int isalpha(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_UC | CT_LC);
}

int isblank(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_BL);
}

int iscntrl(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_CT);
}

int isdigit(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_DD);
}

int isgraph(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_GR);
}

int islower(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_LC);
}

int isprint(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_CT | CT_WS);
}

int ispunct(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_PT);
}

int isspace(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_SP);
}

int isupper(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_UC);
}

int isxdigit(int codepoint)
{
    if(codepoint < 0x00) {
        return 0;
    }

    return ctype[codepoint % CTYPE_SIZE].flags & (CT_XD);
}

int tolower(int codepoint)
{
    if(codepoint < 0x00) {
        return codepoint;
    }

    return ctype[codepoint % CTYPE_SIZE].lower;
}

int toupper(int codepoint)
{
    if(codepoint < 0x00) {
        return codepoint;
    }

    return ctype[codepoint % CTYPE_SIZE].upper;
}

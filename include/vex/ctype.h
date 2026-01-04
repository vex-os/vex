#ifndef _VEX_CTYPE_H
#define _VEX_CTYPE_H 1

extern int isalnum(int codepoint);
extern int isalpha(int codepoint);
extern int isblank(int codepoint);
extern int iscntrl(int codepoint);
extern int isdigit(int codepoint);
extern int isgraph(int codepoint);
extern int islower(int codepoint);
extern int isprint(int codepoint);
extern int ispunct(int codepoint);
extern int isspace(int codepoint);
extern int isupper(int codepoint);
extern int isxdigit(int codepoint);

extern int tolower(int codepoint);
extern int toupper(int codepoint);

#endif

#include <ctype.h>

int isdigit(int c) { return (unsigned)c - '0' < 10; }
int isupper(int c) { return (unsigned)c - 'A' < 26; }
int islower(int c) { return (unsigned)c - 'a' < 26; }
int isalpha(int c) { return isupper(c) || islower(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return isdigit(c) || ((unsigned)c | 0x20) - 'a' < 6; }
int isblank(int c) { return c == ' ' || c == '\t'; }
int isspace(int c) { return c == ' ' || (unsigned)c - '\t' < 5; }
int iscntrl(int c) { return (unsigned)c < 0x20 || c == 0x7f; }
int isprint(int c) { return (unsigned)c - 0x20 < 0x5f; }
int isgraph(int c) { return (unsigned)c - 0x21 < 0x5e; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int tolower(int c) { return isupper(c) ? c | 0x20 : c; }
int toupper(int c) { return islower(c) ? c & ~0x20 : c; }
int isascii(int c) { return (unsigned)c < 0x80; }
int toascii(int c) { return c & 0x7f; }

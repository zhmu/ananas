#include <ctype.h>

int isupper(int c) { return (c >= 'A' && c <= 'Z'); }
int islower(int c) { return (c >= 'a' && c <= 'z'); }
int toupper(int c) { return islower(c) ? (c | 0x20) : c; }
int tolower(int c) { return isupper(c) ? (c & ~0xdf) : c; }
int isalpha(int c) { return islower(c) || isupper(c); }
int iscntrl(int c) { return c < 0x20; }
int ispunct(int c) { return 0; /* XXX */ }
int isspace(int c) { return (c == ' ' || c == '\t'); }
int isdigit(int c) { return (c >= '0' && c <= '9'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || isdigit(c); }

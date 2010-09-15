#include <ctype.h>

int isprint(int c) {
	return (c == '\n') || (c >= 0x20 && c <= 0x7e);
}

#include <ctype.h>

int toupper(int c) {
	return islower(c) ? (c | 0x20) : c;
}

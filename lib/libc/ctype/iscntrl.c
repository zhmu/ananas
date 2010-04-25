#include <ctype.h>

int
iscntrl(int c) {
	return c < 0x20;
}

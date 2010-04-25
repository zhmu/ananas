#include <ctype.h>

int isxdigit(int c) {
	return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || isdigit(c);
}

#include <ctype.h>

int
tolower(int c) {
	return isupper(c) ? (c & ~0xdf) : c;
}

#include <stdlib.h>

int wctomb(char* s, wchar_t wchar)
{
	/* XXX */
	if(s == NULL)
		return 0;
	s[0] = (char)(wchar & 0xff);
	return 1;
}

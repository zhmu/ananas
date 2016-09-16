#include <stdlib.h>

int mbtowc ( wchar_t * _PDCLIB_restrict pwc, const char * _PDCLIB_restrict s, size_t n )
{
	if (s == NULL || n < 1)
		return 0;
	
	wchar_t wc = *s;
	if (pwc != NULL)
		*pwc = wc;
	return 1;
}

#include <stdlib.h>

size_t wcstombs( char * _PDCLIB_restrict s, const wchar_t * _PDCLIB_restrict pwcs, size_t n )
{
	size_t amount = 0;
	for (size_t i = 0; i < n; i++, pwcs++) {
		char c;
		amount += wctomb(&c, *pwcs);
		if (s != NULL)
			*s++ = c;
		if (c == L'\0') {
			/* We should null-terminate the result but not count that byte */
			amount--;
			break;
		}
	}
	return amount;
}

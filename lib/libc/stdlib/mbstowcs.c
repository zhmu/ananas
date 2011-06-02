#include <stdlib.h>

size_t mbstowcs( wchar_t * _PDCLIB_restrict pwcs, const char * _PDCLIB_restrict s, size_t n )
{
	size_t amount = 0;
	for (size_t i = 0; i < n; i++, pwcs++) {
		wchar_t wc;
		int result = mbtowc(&wc, s, n);
		if (result == 0) {
			return (size_t)-1;
		}
		s += result;
		n -= result;
		if (pwcs != NULL)
			*pwcs++ = wc;
		if (wc == L'\0') {
			amount--;
			break;
		}
		amount++;
	}
	return amount;
}

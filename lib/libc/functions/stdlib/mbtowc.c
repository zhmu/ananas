#include <stdlib.h>
#include <wchar.h>

int mbtowc(wchar_t *restrict pwc, const char *restrict s, size_t n)
{
    return (int)mbrtowc(pwc, s, n, NULL);
}

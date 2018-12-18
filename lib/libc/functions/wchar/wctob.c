#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

int wctob(wint_t c)
{
    char out[MB_LEN_MAX];
    if (c == WEOF || wctomb(out, c) != 1)
        return EOF;
    return out[0];
}

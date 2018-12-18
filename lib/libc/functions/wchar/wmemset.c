#include <wchar.h>

wchar_t* wmemset(wchar_t* ws, wchar_t wc, size_t n)
{
    wchar_t* p = ws;
    for (/* nothing */; n > 0; n--)
        *p++ = wc;
    return ws;
}

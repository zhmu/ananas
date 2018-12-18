#include <stdlib.h>
#include <errno.h>

size_t mbstowcs(wchar_t* pwcs, const char* s, size_t n)
{
    errno = EILSEQ;
    return (size_t)-1;
}

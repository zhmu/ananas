#include <stdlib.h>
#include <errno.h>

size_t wcstombs(char* s, const wchar_t* pwcs, size_t n)
{
    errno = EILSEQ;
    return (size_t)-1;
}

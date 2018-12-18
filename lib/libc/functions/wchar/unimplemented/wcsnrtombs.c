#include <errno.h>
#include <wchar.h>

size_t wcsnrtombs(char* dst, const wchar_t** src, size_t nwc, size_t len, mbstate_t* ps)
{
    errno = EILSEQ;
    return (size_t)-1;
}

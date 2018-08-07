#include <string.h>
#include "_PDCLIB_locale.h"

int strerror_r(int errnum, char* strerrbuf, size_t buflen)
{
    if ( errnum >= _PDCLIB_ERRNO_MAX )
    {
	errno = EINVAL;
	return -1;
    }

    const char* errmsg = strerror(errnum);
    if (strlen(errmsg) + 1 > buflen)
    {
	errno = EINVAL;
	return -1;
    }
    strcpy(strerrbuf, errmsg);
    return 0;
}

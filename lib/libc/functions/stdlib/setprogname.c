#include <stdlib.h>
#include <string.h>

const char* __progname = NULL;

void setprogname(const char* progname)
{
    const char* ptr = strrchr(progname, '/');
    if (ptr != NULL)
        __progname = ptr + 1;
    else
        __progname = progname;
}

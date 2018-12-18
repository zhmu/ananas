#include <unistd.h>
#include <string.h>

static char s_ttyname[32];

char* ttyname(int fildes)
{
    strcpy(s_ttyname, "console");
    return s_ttyname;
}

#include <stdlib.h>

void _libc_init(int argc, char** argv, char** envp, char** auxv)
{
    setprogname(argv[0]);
}

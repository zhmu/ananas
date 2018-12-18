#include <ananas/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

pid_t wait3(int* status, int options, struct rusage* rusage)
{
    /* XXX we don't support rusage yet */
    if (rusage != NULL)
        memset(rusage, 0, sizeof(struct rusage));
    return waitpid(0, status, options);
}

#include <sched.h>
#include <errno.h>

int sched_yield()
{
    // No errors are defined, so just act as if we did anything
    return 0;
}

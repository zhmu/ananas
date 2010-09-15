#include <signal.h>

int sigismember(const sigset_t* set, int signo)
{
        return ((*set & (1 << signo)) != 0) ? 1 : 0;
}

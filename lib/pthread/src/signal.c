#include <pthread.h>
#include <signal.h>

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oset)
{
    return sigprocmask(how, set, oset);
}

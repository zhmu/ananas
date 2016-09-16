#include <signal.h>

int sigaddset(sigset_t* set, int signo)
{
	*set |= (1 << signo);
	return 0;
}

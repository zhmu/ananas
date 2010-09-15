#include <signal.h>

int sigfillset(sigset_t* set)
{
	*set = (sigset_t)-1;
	return 0;
}

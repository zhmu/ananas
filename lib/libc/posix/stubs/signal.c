#include <signal.h>
#include <stdio.h>

sig_t
signal(int sig, sig_t func)
{
	return SIG_DFL;
}

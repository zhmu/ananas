#include <signal.h>
#include <stdio.h>
#include <_posix/todo.h>

sig_t signal(int sig, sig_t func)
{
	TODO;
	return SIG_DFL;
}

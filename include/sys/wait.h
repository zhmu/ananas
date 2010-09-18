#ifndef __SYS_WAIT_H__
#define __SYS_WAIT_H__

#include <ananas/_types/pid.h>

pid_t wait(int* stat_loc);
pid_t waitpid(pid_t pid, int* stat_loc, int options);

#define WCONTINUED 	0x01
#define WNOHANG		0x02
#define WUNTRACED	0x04

#define W_EXITED	1
#define W_STOPPED	2
#define W_CONTINUED	3

#define WIFEXITED(s)	((((s) >> 8) & 0xff) == W_EXITED)
#define WEXITSTATUS(s)	((s) & 0xff)
#define WIFSIGNALED(s)	0		/* unsupported */
#define WIFSTOPPED(s)	((((s) >> 8) & 0xff) == W_STOPPED)
#define WIFCONTINUED(s)	((((s) >> 8) & 0xff) == W_CONTINUED)

#define WTERMSIG(s)	SIGTERM		/* unsupported */
#define WSTOPSIG(s)	SIGSTOP		/* unsupported */
#define WCOREDUMP(s)	0		/* unsupported */


#endif /* __SYS_WAIT_H__ */

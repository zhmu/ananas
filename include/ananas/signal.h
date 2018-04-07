#ifndef __ANANAS_SIGNAL_H__
#define __ANANAS_SIGNAL_H__

#include <machine/_types.h>
#include <ananas/_types/sigset.h>
#include <ananas/_types/gid.h>
#include <ananas/_types/pid.h>
#include <ananas/_types/uid.h>

typedef void (*sig_t) (int);

#define SIG_ERR ((sig_t)-1)
#define SIG_DFL ((sig_t)0)
#define SIG_IGN ((sig_t)1)

union sigval {
	int		sival_int;
	void*		sival_ptr;
};

typedef struct {
	int		si_signo;
	int		si_code;
	int		si_errno;
	pid_t		si_pid;
	uid_t		si_uid;
	void*		si_addr;
	int		si_status;
	long		si_band;
	union sigval	si_value;
} siginfo_t;

#define SA_NOCHLDSTOP	0
#define SA_ONSTACK	0
#define SA_RESETHAND	0
#define SA_RESTART	0
#define SA_SIGINFO	0
#define SA_NOCLDWAIT	0
#define SA_NODEFER	0

struct sigaction {
	void		(*sa_handler)(int);
	sigset_t	sa_mask;
	int		sa_flags;
	void		(*sa_sigaction)(int, siginfo_t*, void*);
};

#define SIGHUP		1
#define SIGINT		2
#define SIGQUIT		3
#define SIGILL		4
#define SIGTRAP		5
#define SIGABRT		6
#define SIGEMT		7
#define SIGFPE		8
#define SIGKILL		9
#define SIGBUS		10
#define SIGSEGV		11
#define SIGSYS		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGURG		16
#define SIGSTOP		17
#define SIGTSTP		18
#define SIGCONT		19
#define SIGCHLD		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGIO		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGINFO		29
#define SIGUSR1		30
#define SIGUSR2		31
#define SIGTHR		32
#define _SIGLAST	SIGTHR

#define SIG_BLOCK	0
#define SIG_SETMASK	1
#define SIG_UNBLOCK	2

#endif /* __ANANAS_SIGNAL_H__ */

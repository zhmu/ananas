#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <machine/_types.h>
#include <ananas/_types/pid.h>
#include <ananas/_types/uid.h>
#include <ananas/signal.h>
#include <sys/cdefs.h>

typedef int sig_atomic_t;

__BEGIN_DECLS

int raise(int sig);
int sigemptyset(sigset_t*);
int sigfillset(sigset_t*);
int sigaddset(sigset_t*, int);
int sigdelset(sigset_t*, int);
int sigprocmask(int how, const sigset_t* set, sigset_t* oset);
int sigaction(int sig, const struct sigaction* act, struct sigaction* oact);
int kill(pid_t pid, int sig);
int sigismember(const sigset_t* set, int signo);
sig_t signal(int sig, sig_t func);

/* BSD extensions */
#define NSIG (_SIGLAST + 1)
extern const char* const sys_siglist[NSIG];

__END_DECLS

#endif /* __SIGNAL_H__ */

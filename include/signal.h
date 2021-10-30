/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <sys/types.h>
#include <sys/cdefs.h>
#include <ananas/signal.h>

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

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oset);

/* BSD extensions */
#define NSIG (_SIGLAST + 1)
extern const char* const sys_siglist[NSIG];

__END_DECLS

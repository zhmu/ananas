/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SYS_WAIT_H__
#define __SYS_WAIT_H__

#include <ananas/_types/pid.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

pid_t wait(int* stat_loc);
pid_t waitpid(pid_t pid, int* stat_loc, int options);

__END_DECLS

#define WCONTINUED 0x01
#define WNOHANG 0x02
#define WUNTRACED 0x04

// TODO: merge with ananas/thread.h
#define W_EXITED 0
#define W_SIGNALLED 1
#define W_STOPPED 2
#define W_CONTINUED 3

#define WIFEXITED(s) ((((s) >> 24) & 0xff) == W_EXITED)
#define WEXITSTATUS(s) ((s)&0xffffff)
#define WIFSIGNALED(s) ((((s) >> 24) & 0xff) == W_SIGNALLED)
#define WTERMSIG(s) ((s)&0xffffff)
#define WIFSTOPPED(s) ((((s) >> 24) & 0xff) == W_STOPPED)
#define WSTOPSIG(s) ((s)&0xffffff)
#define WIFCONTINUED(s) ((((s) >> 24) & 0xff) == W_CONTINUED)

#define WCOREDUMP(s) 0 /* unsupported for now */

#endif /* __SYS_WAIT_H__ */

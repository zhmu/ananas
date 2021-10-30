/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SCHED_H__
#define __SCHED_H__

#include <sys/types.h>
#include <time.h>

struct sched_param {
    int sched_priority;
};

#define SCHED_FIFO 0
#define SCHED_RR 1
#define SCHED_OTHER 2

int sched_get_priority_max(int policy);
int sched_get_priority_min(int policy);
int sched_getparam(pid_t pid, struct sched_param* param);
int sched_setparam(pid_t pid, const struct sched_param* param);

int sched_getscheduler(pid_t pid);
int sched_setscheduler(pid_t pid, int policy, const struct sched_param* param);

int sched_rr_get_interval(pid_t pid, struct timespec* interval);
int sched_yield(void);

#endif /* __SCHED_H__*/

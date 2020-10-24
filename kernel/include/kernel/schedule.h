/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <ananas/util/list.h>

struct Thread;

struct SchedulerPriv : util::List<SchedulerPriv>::NodePtr {
    Thread* sp_thread; /* Backreference to the thread */
};
typedef util::List<SchedulerPriv> SchedulerPrivList;

namespace scheduler
{
    void InitThread(Thread& t);
    void ResumeThread(Thread& t);
    void SuspendThread(Thread& t);
    void ExitThread(Thread& t);

    void Schedule();
    void Launch();

    void Deactivate();
    bool IsActive();

} // namespace scheduler

#endif /* __SCHEDULE_H__ */

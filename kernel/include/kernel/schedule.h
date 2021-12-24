/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/util/list.h>

struct Thread;

namespace scheduler
{
    void InitThread(Thread& t);
    void ResumeThread(Thread& t);
    void WaitUntilReleased(Thread& t);

    void Schedule();
    void Launch();

    void Deactivate();
    bool IsActive();

} // namespace scheduler

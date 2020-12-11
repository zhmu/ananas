/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * The reaper is responsible for killing off threads which need to be
 * destroyed - a thread cannot completely exit by itself as it can't
 * free things like its stack.
 *
 * This is commonly used for kernel threads; user threads are generally
 * destroyed by their parent wait()-ing for them.
 */
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/reaper.h"
#include "kernel/result.h"
#include "kernel/thread.h"

void reaper_enqueue(Thread& t)
{
    panic("reaper_enqueue");
}

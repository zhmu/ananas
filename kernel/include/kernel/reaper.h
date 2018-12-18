/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __REAPER_H__
#define __REAPER_H__

struct Thread;

void reaper_enqueue(Thread& t);

#endif /* __REAPER_H__ */

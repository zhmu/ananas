/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_ANKFS_PROC_H
#define ANANAS_ANKFS_PROC_H

#include <ananas/types.h>

namespace ankhfs
{
    class IAnkhSubSystem;

    IAnkhSubSystem& GetProcSubSystem();

} // namespace ankhfs

#endif // ANANAS_ANKFS_PROC_H

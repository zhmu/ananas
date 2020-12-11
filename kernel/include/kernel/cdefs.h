/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#define __nonnull __attribute((nonnull))
#define __malloc __attribute__((malloc))
#define __unused __attribute__((unused))

#define va_arg __builtin_va_arg
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_list __builtin_va_list

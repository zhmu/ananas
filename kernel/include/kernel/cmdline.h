/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_CMDLINE_H__
#define __ANANAS_CMDLINE_H__

void cmdline_init(char* bootargs);
const char* cmdline_get_string(const char* key);

#endif /* __ANANAS_CMDLINE_H__ */

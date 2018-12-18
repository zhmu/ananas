/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_DEBUG_CONSOLE_H__
#define __ANANAS_DEBUG_CONSOLE_H__

extern "C" {
void debugcon_init();
void debugcon_putch(int c);
int debugcon_getch();
}

#endif /* __ANANAS_DEBUG_CONSOLE_H__ */

/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/stdint.h>

class Device;

void console_putchar(int c);
void console_putstring(const char* s);
uint8_t console_getchar();

extern Device* console_tty;

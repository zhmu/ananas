/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#define GDB_NUMREGS 56
#define GDB_REG_PC 16

size_t gdb_md_get_register_size(int regnum);
void* gdb_md_get_register(struct STACKFRAME* sf, int regnum);
int gdb_md_map_signal(struct STACKFRAME* sf);

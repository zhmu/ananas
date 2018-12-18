/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SETJMP_H__
#define __SETJMP_H__

#include <sys/cdefs.h>

/* jmp_buf is architecture-dependant */
#include <machine/setjmp.h>

__BEGIN_DECLS

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

__END_DECLS

#endif /* __SETJMP_H__ */

/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * Low-level initialization code.
 *
 */
.text

#include <sys/types.h>
#include "options.h"

.global __entry, bootstrap_stack

__entry:
    movq    $bootstrap_stack, %rsp

#ifdef OPTION_DEBUG_CONSOLE
    /*
     * Make sure we have a debugger console available immediately; note that we must
     * store %rdi as it contains the bootinfo parameter.
     */
    pushq   %rdi
    call    debugcon_init
    popq    %rdi
#endif

    call    md_startup

    /* why are we here? */
    hlt

.align 32

.data

    .space  0x2000
bootstrap_stack:

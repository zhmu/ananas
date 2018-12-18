/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * Final object file; this is used to place trailing code after the
 * .init and .fini sections.
 */
.section .init,"ax",@progbits
    addq    $8, %rsp
    ret

.section .fini,"ax",@progbits
    addq    $8, %rsp
    ret

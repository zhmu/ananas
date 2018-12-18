/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
.section .ctors, "aw", @progbits
.align 8
.global __CTOR_LIST_END__
.hidden __CTOR_LIST_END__
__CTOR_LIST_END__:
    .quad   0

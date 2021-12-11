/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */

/* Refer to https://www.netbsd.org/docs/kernel/elf-notes.html for the format in use */

    .section ".note.ananas.ident", "a"
    .p2align 2
    .long   2f - 1f
    .long   4f - 3f
    .long   1       /* note type: NT_ANANAS_ABI_TAG */
1:  .asciz  "Ananas"
2:  .p2align 2
3:  .long   0       /* will become version some day */
4:  .p2align 2

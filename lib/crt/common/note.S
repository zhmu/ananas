/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
.section ".note.tag", "a"
.p2align    2   /* align to 8 bytes */

.long   7       /* name size */
.long   4       /* description size */
.long   1       /* note type */
.ascii  "Ananas\0"
.long   0       /* will become version some day */

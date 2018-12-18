/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
// SUMMARY:mmap()-ing readonly is actually readonly
// PROVIDE-FILE: "mmap-3.txt" "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

#include "framework.h"
#include <fcntl.h>
#include <sys/mman.h>

TEST_BODY_BEGIN
{
    int fd = open("mmap-3.txt", O_RDONLY);
    ASSERT_NE(-1, fd);

    void* p = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    ASSERT_NE(MAP_FAILED, p);

    volatile int* i = (int*)p;
    ASSERT_DEATH(*i = 'A');
}
TEST_BODY_END

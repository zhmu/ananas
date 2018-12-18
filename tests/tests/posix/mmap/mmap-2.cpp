/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
// SUMMARY:mmap() with a smaller file fills the rest with zeros
// PROVIDE-FILE: "mmap-2.txt" "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

#include "framework.h"
#include <fcntl.h>
#include <sys/mman.h>

TEST_BODY_BEGIN
{
    int fd = open("mmap-2.txt", O_RDONLY);
    ASSERT_NE(-1, fd);

    void* p = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    ASSERT_NE(MAP_FAILED, p);

    char* c = (char*)p;
    int n = 0;
    for (/* nothing */; n < 26; n++, c++) {
        ASSERT_EQ(*c, n + 'A');
    }
    for (/* nothing */; n < 4096; n++, c++) {
        ASSERT_EQ(*c, '\0');
    }
}
TEST_BODY_END

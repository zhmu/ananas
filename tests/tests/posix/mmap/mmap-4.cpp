/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
// SUMMARY:mmap()-ing more than file size silently truncates
// PROVIDE-FILE: "mmap-4.txt" "ABCD"

#include "framework.h"
#include <fcntl.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096

TEST_BODY_BEGIN
{
    int fd = open("mmap-4.txt", O_RDONLY);
    ASSERT_NE(-1, fd);

    void* p = mmap(nullptr, PAGE_SIZE * 2, PROT_READ, MAP_PRIVATE, fd, 0);
    ASSERT_NE(MAP_FAILED, p);

    char* c = (char*)p;
    int n = 0;
    for (/* nothing */; n < 4; n++, c++) {
        ASSERT_EQ(*c, n + 'A');
    }
    for (/* nothing */; n < PAGE_SIZE; n++, c++) {
        ASSERT_EQ(*c, '\0');
    }

    volatile int* i = (int*)c;
    ASSERT_DEATH(n = *i);
    EXPECT_EQ(n, 0);
}
TEST_BODY_END

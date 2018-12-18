/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
// SUMMARY:Readonly mappings are inherited readonly
// PROVIDE-FILE: "mmap-6.txt" "ABCD"

#include "framework.h"
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

TEST_BODY_BEGIN
{
    int fd = open("mmap-6.txt", O_RDONLY);
    ASSERT_NE(-1, fd);

    void* p = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    ASSERT_NE(MAP_FAILED, p);

    int pid = fork();
    if (pid == 0) {
        volatile int* i = (int*)p;
        *i = 'A'; // this should crash
        ASSERT_FAILURE;
    } else {
        int stat;
        wait(&stat);

        // We expect the child to exit with a signal
        EXPECT_NE(0, WIFSIGNALED(stat));
    }
}
TEST_BODY_END

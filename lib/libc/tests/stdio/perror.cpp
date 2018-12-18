#include <gtest/gtest.h>
#include <stdio.h>
#include <limits.h>

static char const testfile[] = "testing/testfile";

TEST(stdio, perror)
{
    int fd_stderr = dup(STDERR_FILENO);
    ASSERT_GT(fd_stderr, -1);

    char buffer[100];

    // XXXRS Why?
    sprintf(buffer, "%llu", ULLONG_MAX);
    EXPECT_EQ(ULLONG_MAX, strtoull(buffer, NULL, 10));

    FILE* fh = freopen(testfile, "wb+", stderr);
    ASSERT_NE(nullptr, fh);

    perror("Test");
    rewind(fh);
    EXPECT_EQ(7, fread(buffer, 1, 7, fh));
    EXPECT_EQ(0, memcmp(buffer, "Test: ", 6));
    EXPECT_EQ(0, fclose(fh));
    EXPECT_EQ(0, remove(testfile));

    stderr = fdopen(fd_stderr, "w");
}

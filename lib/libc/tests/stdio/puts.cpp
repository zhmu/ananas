#include <gtest/gtest.h>
#include <stdio.h>

static char const testfile[] = "testing/testfile";

TEST(stdio, puts)
{
    int fd_stdout = dup(STDOUT_FILENO);
    ASSERT_GT(fd_stdout, -1);

    char const* message = "SUCCESS testing puts()";
    char buffer[23];
    buffer[22] = 'x';

    FILE* fh = freopen(testfile, "wb+", stdout);
    ASSERT_NE(nullptr, fh);

    EXPECT_GE(puts(message), 0);
    rewind(fh);
    EXPECT_EQ(22, fread(buffer, 1, 22, fh));
    EXPECT_EQ(0, memcmp(buffer, message, 22));
    EXPECT_EQ('x', buffer[22]);
    EXPECT_EQ(0, fclose(fh));
    EXPECT_EQ(0, remove(testfile));

    stdout = fdopen(fd_stdout, "w");
}

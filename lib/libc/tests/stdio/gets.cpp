#include <gtest/gtest.h>
#include <stdio.h>

// TODO: How to drag this in? It's officially deprecated in C++14 ...

#if 0
static char const testfile[]="testing/testfile";

TEST(stdio, fputs)
{
    int fd_stdin = dup(STDIN_FILENO);
    ASSERT_GT(fd_stdin, -1);

    char const * gets_test = "foo\nbar\0baz\nweenie";

    char buffer[10];
    {
        FILE* fh = fopen(testfile, "wb");
        ASSERT_NE(nullptr, fh);
        EXPECT_EQ(18, fwrite(gets_test, 1, 18, fh));
        EXPECT_EQ(0, fclose(fh));
    }
    {
        FILE* fh = freopen(testfile, "rb", stdin);
        ASSERT_NE(nullptr, fh);
        EXPECT_EQ(buffer, gets(buffer));
        EXPECT_EQ(0, strcmp(buffer, "foo"));
        EXPECT_EQ(buffer, gets(buffer));
        EXPECT_EQ(0, memcmp(buffer, "bar\0baz\0", 8));
        EXPECT_EQ(buffer, gets(buffer));
        EXPECT_EQ(0, strcmp(buffer, "weenie"))
        EXPECT_TRUE(feof(fh));
        EXPECT_EQ(0, fseek(fh, -1, SEEK_END));
        EXPECT_EQ(buffer, gets(buffer));
        EXPECT_EQ(0, strcmp(buffer, "e"));
        EXPECT_TRUE(feof(fh));
        EXPECT_EQ(0, fseek(fh, 0, SEEK_END));
        EXPECT_EQ(nullptr, gets(buffer));
        EXPECT_EQ(0, fclose(fh));
        EXPECT_EQ(0, remove(testfile));
    }

    stdin = fdopen(fd_stdin, "w");
}
#endif

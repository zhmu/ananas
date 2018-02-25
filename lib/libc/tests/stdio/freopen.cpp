#include <gtest/gtest.h>
#include <stdio.h>

static char const testfile1[]="testing/testfile1";
static char const testfile2[]="testing/testfile2";

TEST(stdio, freopen)
{
    int fd_stdin = dup(STDIN_FILENO);
    ASSERT_GT(fd_stdin, -1);
    int fd_stdout = dup(STDOUT_FILENO);
    ASSERT_GT(fd_stdout, -1);

    FILE * fin = fopen(testfile1, "wb+");
    ASSERT_NE(nullptr, fin);
    EXPECT_EQ('x', fputc('x', fin));
    EXPECT_EQ(0, fclose(fin));

    fin = freopen(testfile1, "rb", stdin);
    ASSERT_NE(nullptr, fin);
    EXPECT_EQ('x', getchar());

    FILE* fout = freopen( testfile2, "wb+", stdout);
    ASSERT_NE(nullptr, fout);
    EXPECT_EQ('x', putchar('x'));
    rewind(fout);
    EXPECT_EQ('x', fgetc(fout));

    EXPECT_EQ(0, fclose(fin));
    EXPECT_EQ(0, fclose(fout));
    EXPECT_EQ(0, remove(testfile1));
    EXPECT_EQ(0, remove(testfile2));

    stdin = fdopen(fd_stdin, "w");
    stdout = fdopen(fd_stdout, "w");
}

#include <gtest/gtest.h>
#include <stdio.h>

static char const testfile[] = "testing/testfile";

TEST(stdio, fgets)
{
    char const* gets_test = "foo\nbar\0baz\nweenie";
    char buffer[10];

    FILE* fh = fopen(testfile, "wb+");
    ASSERT_NE(nullptr, fh);

    EXPECT_EQ(18, fwrite(gets_test, 1, 18, fh));
    rewind(fh);
    EXPECT_EQ(buffer, fgets(buffer, 10, fh));
    EXPECT_EQ(0, strcmp(buffer, "foo\n"));
    EXPECT_EQ(buffer, fgets(buffer, 10, fh));
    EXPECT_EQ(0, memcmp(buffer, "bar\0baz\n", 8));
    EXPECT_EQ(buffer, fgets(buffer, 10, fh));
    EXPECT_EQ(0, strcmp(buffer, "weenie"));
    EXPECT_TRUE(feof(fh));
    EXPECT_EQ(0, fseek(fh, -1, SEEK_END));
    EXPECT_EQ(buffer, fgets(buffer, 1, fh));
    EXPECT_EQ(0, strcmp(buffer, ""));
    EXPECT_EQ(NULL, fgets(buffer, 0, fh));
    EXPECT_FALSE(feof(fh));
    EXPECT_EQ(buffer, fgets(buffer, 1, fh));
    EXPECT_EQ(0, strcmp(buffer, ""));
    EXPECT_FALSE(feof(fh));
    EXPECT_EQ(buffer, fgets(buffer, 2, fh));
    EXPECT_EQ(0, strcmp(buffer, "e"));
    EXPECT_EQ(0, fseek(fh, 0, SEEK_END));
    EXPECT_EQ(NULL, fgets(buffer, 2, fh));
    EXPECT_TRUE(feof(fh));
    EXPECT_EQ(0, fclose(fh));
    EXPECT_EQ(0, remove(testfile));
}

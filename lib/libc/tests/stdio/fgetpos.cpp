#include <gtest/gtest.h>
#include <stdio.h>

static char const teststring[] =
    "1234567890\nABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n";

TEST(stdio, fgetpos)
{
    size_t teststring_len = strlen(teststring);

    FILE* fh = tmpfile();
    ASSERT_NE(nullptr, fh);

    fpos_t pos1, pos2;
    EXPECT_EQ(0, fgetpos(fh, &pos1));
    EXPECT_EQ(teststring_len, fwrite(teststring, 1, teststring_len, fh));
    EXPECT_EQ(teststring_len, (size_t)ftell(fh));
    EXPECT_EQ(0, fgetpos(fh, &pos2));
    EXPECT_EQ(0, fsetpos(fh, &pos1));
    EXPECT_EQ(0, ftell(fh));
    EXPECT_EQ(0, fsetpos(fh, &pos2));
    EXPECT_EQ(teststring_len, (size_t)ftell(fh));
    EXPECT_EQ(0, fclose(fh));
}

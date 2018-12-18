#include <gtest/gtest.h>
#include <stdio.h>

static char const teststring[] =
    "1234567890\nABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n";

TEST(stdio, fseek)
{
    FILE* fh = tmpfile();
    ASSERT_NE(nullptr, fh);
    EXPECT_EQ(strlen(teststring), fwrite(teststring, 1, strlen(teststring), fh));
    size_t teststring_len = strlen(teststring);
    /* General functionality */
    EXPECT_EQ(0, fseek(fh, -1, SEEK_END));
    EXPECT_EQ(teststring_len - 1, (size_t)ftell(fh));
    EXPECT_EQ(0, fseek(fh, 0, SEEK_END));
    EXPECT_EQ(teststring_len, (size_t)ftell(fh));
    EXPECT_EQ(0, fseek(fh, 0, SEEK_SET));
    EXPECT_EQ(0, ftell(fh));
    EXPECT_EQ(0, fseek(fh, 5, SEEK_CUR));
    EXPECT_EQ(5, ftell(fh));
    EXPECT_EQ(0, fseek(fh, -3, SEEK_CUR));
    EXPECT_EQ(2, ftell(fh));
    /* Checking behaviour around EOF */
    EXPECT_EQ(0, fseek(fh, 0, SEEK_END));
    EXPECT_FALSE(feof(fh));
    EXPECT_EQ(EOF, fgetc(fh));
    EXPECT_TRUE(feof(fh));
    EXPECT_EQ(0, fseek(fh, 0, SEEK_END));
    EXPECT_FALSE(feof(fh));
    /* Checking undo of ungetc() */
    EXPECT_EQ(0, fseek(fh, 0, SEEK_SET));
    EXPECT_EQ(teststring[0], fgetc(fh));
    EXPECT_EQ(teststring[1], fgetc(fh));
    EXPECT_EQ(teststring[2], fgetc(fh));
    EXPECT_EQ(3, ftell(fh));
    EXPECT_EQ(teststring[2], ungetc(teststring[2], fh));
    EXPECT_EQ(2, ftell(fh));
    EXPECT_EQ(teststring[2], fgetc(fh));
    EXPECT_EQ(3, ftell(fh));
    EXPECT_EQ('x', ungetc('x', fh));
    EXPECT_EQ(2, ftell(fh));
    EXPECT_EQ('x', fgetc(fh));
    EXPECT_EQ('x', ungetc('x', fh));
    EXPECT_EQ(2, ftell(fh));
    EXPECT_EQ(0, fseek(fh, 2, SEEK_SET));
    EXPECT_EQ(teststring[2], fgetc(fh));
    /* PDCLIB-7: Check that we handle the underlying file descriptor correctly
     *           in the SEEK_CUR case */
    EXPECT_EQ(0, fseek(fh, 10, SEEK_SET));
    EXPECT_EQ(10L, ftell(fh));
    EXPECT_EQ(0, fseek(fh, 0, SEEK_CUR));
    EXPECT_EQ(10L, ftell(fh));
    EXPECT_EQ(0, fseek(fh, 2, SEEK_CUR));
    EXPECT_EQ(12L, ftell(fh));
    EXPECT_EQ(0, fseek(fh, -1, SEEK_CUR));
    EXPECT_EQ(11L, ftell(fh));
    /* Checking error handling */
    EXPECT_EQ(-1, fseek(fh, -5, SEEK_SET));
    EXPECT_EQ(0, fseek(fh, 0, SEEK_END));
    EXPECT_EQ(0, fclose(fh));
}

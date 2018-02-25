#include <gtest/gtest.h>
#include <string.h>

TEST(string, strxfrm)
{
    char s[] = "xxxxxxxxxxx";
    EXPECT_EQ(12, strxfrm(NULL, "123456789012", 0));
    EXPECT_EQ(12, strxfrm(s, "123456789012", 12));
    EXPECT_EQ(10, strxfrm(s, "1234567890", 11));
    EXPECT_EQ('1', s[0]);
    EXPECT_EQ('0', s[9]);
    EXPECT_EQ('\0', s[10]);
}

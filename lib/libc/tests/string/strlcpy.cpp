#include <gtest/gtest.h>
#include <string.h>

#ifdef _PDCLIB_C_VERSION
TEST(string, strlcpy)
{
    char destbuf[10];
    EXPECT_EQ(2, strlcpy(NULL, "a", 0));
    EXPECT_EQ(2, strlcpy(destbuf, "a", 10));
    EXPECT_EQ(0, strcmp(destbuf, "a"));
    EXPECT_EQ(11, strlcpy(destbuf, "helloworld", 10));
    EXPECT_EQ(0, strcmp(destbuf, "helloworl"));
}
#endif

#include <gtest/gtest.h>
#include <string.h>

#ifdef _PDCLIB_C_VERSION
TEST(string, strlcat)
{
    char dstbuf[16];

    strcpy(dstbuf, "hi");
    EXPECT_EQ(3, strlcat(dstbuf, "", 16));
    EXPECT_EQ(5, strlcat(dstbuf, "hi", 16));
    EXPECT_EQ(17, strlcat(dstbuf, "hello, world", 16));
    EXPECT_EQ(18, strlcat(dstbuf, "hi", 16));
    EXPECT_EQ(0, strcmp(dstbuf, "hihihello, worl"));
}
#endif

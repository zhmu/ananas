#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswalnum)
{
    EXPECT_TRUE(iswalnum(L'a'));
    EXPECT_TRUE(iswalnum(L'z'));
    EXPECT_TRUE(iswalnum(L'E'));
    EXPECT_TRUE(iswalnum(L'3'));
    EXPECT_FALSE(iswalnum(L';'));
}

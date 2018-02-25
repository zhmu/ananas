#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswalpha)
{
    EXPECT_TRUE(iswalpha(L'a'));
    EXPECT_TRUE(iswalpha(L'z'));
    EXPECT_TRUE(iswalpha(L'E'));
    EXPECT_FALSE(iswalpha(L'3'));
    EXPECT_FALSE(iswalpha(L';'));
}

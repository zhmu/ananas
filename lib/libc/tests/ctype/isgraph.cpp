#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isgraph)
{
    EXPECT_TRUE(isgraph('a'));
    EXPECT_TRUE(isgraph('z'));
    EXPECT_TRUE(isgraph('A'));
    EXPECT_TRUE(isgraph('Z'));
    EXPECT_TRUE(isgraph('@'));
    EXPECT_FALSE(isgraph('\t'));
    EXPECT_FALSE(isgraph('\0'));
    EXPECT_FALSE(isgraph(' '));
}

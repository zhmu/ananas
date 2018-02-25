#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, iscntrl)
{
    EXPECT_TRUE(iscntrl('\a'));
    EXPECT_TRUE(iscntrl('\b'));
    EXPECT_TRUE(iscntrl('\n'));
    EXPECT_FALSE(iscntrl(' '));
}

#include <gtest/gtest.h>
#include <stdio.h>

TEST(stdio, tmpnam)
{
    EXPECT_LT(strlen(tmpnam(NULL)), L_tmpnam);
}

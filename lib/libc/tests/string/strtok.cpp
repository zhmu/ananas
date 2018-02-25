#include <gtest/gtest.h>
#include <string.h>

TEST(string, strtok)
{
    char s[] = "_a_bc__d_";
    EXPECT_EQ(&s[1], strtok(s, "_"));
    EXPECT_EQ('a', s[1]);
    EXPECT_EQ('\0', s[2]);
    EXPECT_EQ(&s[3], strtok(NULL, "_"));
    EXPECT_EQ('b', s[3]);
    EXPECT_EQ('c', s[4]);
    EXPECT_EQ('\0', s[5]);
    EXPECT_EQ(&s[7], strtok(NULL, "_"));
    EXPECT_EQ('_', s[6]);
    EXPECT_EQ('d', s[7]);
    EXPECT_EQ('\0', s[8]);
    EXPECT_EQ(NULL, strtok(NULL, "_"));
    strcpy(s, "ab_cd");
    EXPECT_EQ(&s[0], strtok(s, "_"));
    EXPECT_EQ('a', s[0]);
    EXPECT_EQ('b', s[1]);
    EXPECT_EQ('\0', s[2]);
    EXPECT_EQ(&s[3], strtok(NULL, "_"));
    EXPECT_EQ('c', s[3]);
    EXPECT_EQ('d', s[4]);
    EXPECT_EQ('\0', s[5]);
    EXPECT_EQ(NULL, strtok(NULL, "_"));
}

#include <gtest/gtest.h>
#include <string.h>

TEST(string, strstr)
{
    char s[] = "abcabcabcdabcde";
    EXPECT_EQ(NULL, strstr(s, "x"));
    EXPECT_EQ(NULL, strstr(s, "xyz"));
    EXPECT_EQ(&s[0], strstr(s, "a"));
    EXPECT_EQ(&s[0], strstr(s, "abc"));
    EXPECT_EQ(&s[6], strstr(s, "abcd"));
    EXPECT_EQ(&s[10], strstr(s, "abcde"));
}

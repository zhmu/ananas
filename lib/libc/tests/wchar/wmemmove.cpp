#include <gtest/gtest.h>
#include <wchar.h>

TEST(wchar, wmemmove)
{
    wchar_t s[] = L"xxxxabcde";
    EXPECT_EQ(s, wmemmove(s, s + 4, 5));
    EXPECT_EQ(L'a', s[0]);
    EXPECT_EQ(L'e', s[4]);
    EXPECT_EQ(L'b', s[5]);
    EXPECT_EQ(s + 4, wmemmove(s + 4, s, 5));
    EXPECT_EQ(L'a', s[4]);
}

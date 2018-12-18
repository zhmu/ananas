#include <gtest/gtest.h>
#include <wchar.h>

TEST(wchar, wcsstr)
{
    wchar_t s[] = L"abcabcabcdabcde";
    EXPECT_EQ(NULL, wcsstr(s, L"x"));
    EXPECT_EQ(NULL, wcsstr(s, L"xyz"));
    EXPECT_EQ(&s[0], wcsstr(s, L"a"));
    EXPECT_EQ(&s[0], wcsstr(s, L"abc"));
    EXPECT_EQ(&s[6], wcsstr(s, L"abcd"));
    EXPECT_EQ(&s[10], wcsstr(s, L"abcde"));
}

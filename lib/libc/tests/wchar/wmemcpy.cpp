#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wmemcpy)
{
    wchar_t s[] = L"xxxxxxxxxxx";
    EXPECT_EQ(s, wmemcpy(s, wabcde, 6));
    EXPECT_EQ(L'e', s[4]);
    EXPECT_EQ(L'\0', s[5]);
    EXPECT_EQ(s + 5, wmemcpy(s + 5, wabcde, 5));
    EXPECT_EQ(L'e', s[9]);
    EXPECT_EQ(L'x', s[10]);
}

#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wcscat)
{
    wchar_t s[] = L"xx\0xxxxxx";
    EXPECT_EQ(s, wcscat(s, wabcde));
    EXPECT_EQ(L'a', s[2]);
    EXPECT_EQ(L'e', s[6]);
    EXPECT_EQ(L'\0', s[7]);
    EXPECT_EQ(L'x', s[8]);
    s[0] = L'\0';
    EXPECT_EQ(s, wcscat(s, wabcdx));
    EXPECT_EQ(L'x', s[4]);
    EXPECT_EQ(L'\0', s[5]);
    EXPECT_EQ(s, wcscat(s, L"\0"));
    EXPECT_EQ(L'\0', s[5]);
    EXPECT_EQ(L'e', s[6]);
}

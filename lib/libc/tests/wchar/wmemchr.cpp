#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wmemchr)
{
    EXPECT_EQ(&wabcde[2], wmemchr(wabcde, L'c', 5));
    EXPECT_EQ(&wabcde[0], wmemchr(wabcde, L'a', 1));
    EXPECT_EQ(NULL, wmemchr(wabcde, L'a', 0));
    EXPECT_EQ(NULL, wmemchr(wabcde, L'\0', 5));
    EXPECT_EQ(&wabcde[5], wmemchr(wabcde, L'\0', 6));
}

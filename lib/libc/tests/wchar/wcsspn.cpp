#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wcsspn)
{
    EXPECT_EQ(3, wcsspn(wabcde, L"abc"));
    EXPECT_EQ(0, wcsspn(wabcde, L"b"));
    EXPECT_EQ(5, wcsspn(wabcde, wabcde));
}

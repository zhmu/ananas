#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wcscspn)
{
    EXPECT_EQ(5, wcscspn(wabcde, L"x"));
    EXPECT_EQ(5, wcscspn(wabcde, L"xyz"));
    EXPECT_EQ(5, wcscspn(wabcde, L"zyx"));
    EXPECT_EQ(4, wcscspn(wabcdx, L"x"));
    EXPECT_EQ(4, wcscspn(wabcdx, L"xyz"));
    EXPECT_EQ(4, wcscspn(wabcdx, L"zyx"));
    EXPECT_EQ(0, wcscspn(wabcde, L"a"));
    EXPECT_EQ(0, wcscspn(wabcde, L"abc"));
    EXPECT_EQ(0, wcscspn(wabcde, L"cba"));
}

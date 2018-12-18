#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wcspbrk)
{
    EXPECT_EQ(NULL, wcspbrk(wabcde, L"x"));
    EXPECT_EQ(NULL, wcspbrk(wabcde, L"xyz"));
    EXPECT_EQ(&wabcdx[4], wcspbrk(wabcdx, L"x"));
    EXPECT_EQ(&wabcdx[4], wcspbrk(wabcdx, L"xyz"));
    EXPECT_EQ(&wabcdx[4], wcspbrk(wabcdx, L"zyx"));
    EXPECT_EQ(&wabcde[0], wcspbrk(wabcde, L"a"));
    EXPECT_EQ(&wabcde[0], wcspbrk(wabcde, L"abc"));
    EXPECT_EQ(&wabcde[0], wcspbrk(wabcde, L"cba"));
}

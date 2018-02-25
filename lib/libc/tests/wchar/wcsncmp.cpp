#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wcsncmp)
{
    wchar_t cmpabcde[] = L"abcde\0f";
    wchar_t cmpabcd_[] = L"abcde\xfc";
    wchar_t empty[] = L"";
    wchar_t x[] = L"x";
    EXPECT_EQ(0, wcsncmp( wabcde, cmpabcde, 5));
    EXPECT_EQ(0, wcsncmp( wabcde, cmpabcde, 10));
    EXPECT_LT(wcsncmp( wabcde, wabcdx, 5), 0);
    EXPECT_GT(wcsncmp( wabcdx, wabcde, 5), 0);
    EXPECT_LT(wcsncmp( empty, wabcde, 5), 0);
    EXPECT_GT(wcsncmp( wabcde, empty, 5), 0);
    EXPECT_EQ(0, wcsncmp( wabcde, wabcdx, 4));
    EXPECT_EQ(0, wcsncmp( wabcde, x, 0));
    EXPECT_LT(wcsncmp( wabcde, x, 1), 0);
    EXPECT_LT(wcsncmp( wabcde, cmpabcd_, 10 ), 0);
}

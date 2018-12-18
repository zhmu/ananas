#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wcscmp)
{
    wchar_t cmpabcde[] = L"abcde";
    wchar_t cmpabcd_[] = L"abcd\xfc";
    wchar_t empty[] = L"";
    EXPECT_EQ(0, wcscmp(wabcde, cmpabcde));
    EXPECT_LT(wcscmp(wabcde, wabcdx), 0);
    EXPECT_GT(wcscmp(wabcdx, wabcde), 0);
    EXPECT_LT(wcscmp(empty, wabcde), 0);
    EXPECT_GT(wcscmp(wabcde, empty), 0);
    EXPECT_LT(wcscmp(wabcde, cmpabcd_), 0);
}

#include <gtest/gtest.h>
#include <wchar.h>

static wchar_t const wabcde[] = L"abcde";
static wchar_t const wabcdx[] = L"abcdx";

TEST(wchar, wmemcmp)
{
    wchar_t const xxxxx[] = L"xxxxx";
    EXPECT_LT(wmemcmp(wabcde, wabcdx, 5), 0);
    EXPECT_EQ(0, wmemcmp(wabcde, wabcdx, 4));
    EXPECT_EQ(0, wmemcmp(wabcde, xxxxx, 0));
    EXPECT_GT(wmemcmp(xxxxx, wabcde, 1), 0);
}

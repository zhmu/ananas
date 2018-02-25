#include <gtest/gtest.h>
#include <wchar.h>

TEST(wchar, wcstok)
{
    // MinGW at least has a very nonconforming (different signature!) variety
    // of wcstok
    wchar_t s[] = L"_a_bc__d_";
    wchar_t* state  = NULL;

    EXPECT_EQ(&s[1], wcstok(s, L"_", &state));
    EXPECT_EQ(L'a', s[1]);
    EXPECT_EQ(L'\0', s[2]);
    EXPECT_EQ(&s[3], wcstok(NULL, L"_", &state));
    EXPECT_EQ(L'b', s[3]);
    EXPECT_EQ(L'c', s[4]);
    EXPECT_EQ(L'\0', s[5]);

    EXPECT_EQ(&s[7], wcstok(NULL, L"_", &state));
    EXPECT_EQ(L'_', s[6]);
    EXPECT_EQ(L'd', s[7]);
    EXPECT_EQ(L'\0', s[8]);
    EXPECT_EQ(NULL, wcstok(NULL, L"_", &state));
    wcscpy( s, L"ab_cd" );
    EXPECT_EQ(&s[0], wcstok(s, L"_", &state));
    EXPECT_EQ(L'a', s[0]);
    EXPECT_EQ(L'b', s[1]);
    EXPECT_EQ(L'\0', s[2]);
    EXPECT_EQ(&s[3], wcstok(NULL, L"_", &state));
    EXPECT_EQ(L'c', s[3]);
    EXPECT_EQ(L'd', s[4]);
    EXPECT_EQ(L'\0', s[5]);
	EXPECT_EQ(NULL, wcstok(NULL, L"_", &state));
}

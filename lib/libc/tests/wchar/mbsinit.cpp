#include <gtest/gtest.h>
#include <wchar.h>
#include <string.h>

TEST(wchar, mbsinit)
{
    mbstate_t mbs;
    memset(&mbs, 0, sizeof mbs);

    EXPECT_NE(0, mbsinit(NULL));
    EXPECT_NE(0, mbsinit(&mbs));

#ifdef _PDCLIB_C_VERSION
    // Surrogate pending
    mbs._Surrogate = 0xFEED;
    EXPECT_EQ(0, mbsinit(&mbs));

    mbs._Surrogate = 0;
    mbs._PendState = 1;
    EXPECT_EQ(0, mbsinit(&mbs));
#endif
}

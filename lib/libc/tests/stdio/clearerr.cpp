#include <gtest/gtest.h>
#include <stdio.h>

// XXX Marked the test PCLIB-only for now due to the assumption below
#ifdef _PDCLIB_C_VERSION
TEST(stdio, clearerr)
{
    FILE* fh = tmpfile();
    ASSERT_NE(nullptr, fh);

    /* Flags should be clear */
    EXPECT_FALSE(ferror(fh));
    EXPECT_FALSE(feof(fh));
    /* Reading from input stream - should provoke error */
    /* FIXME: Apparently glibc disagrees on this assumption. How to provoke error on glibc? */
    EXPECT_EQ(EOF, fgetc(fh));
    EXPECT_TRUE(ferror(fh));
    EXPECT_FALSE(feof(fh));
    /* clearerr() should clear flags */
    clearerr(fh);
    EXPECT_FALSE(ferror(fh));
    EXPECT_FALSE(feof(fh));
    /* Reading from empty stream - should provoke EOF */
    rewind(fh);
    EXPECT_EQ(EOF, fgetc(fh));
    EXPECT_FALSE(ferror(fh));
    EXPECT_TRUE(feof(fh));
    /* clearerr() should clear flags */
    clearerr(fh);
    EXPECT_FALSE(ferror(fh));
    EXPECT_FALSE(feof(fh));
    EXPECT_EQ(0, fclose(fh));
}
#endif

#include <gtest/gtest.h>
#include <stdio.h>

static char const testfile[]="testing/testfile";

TEST(stdio, fopen)
{
    /* Some of the tests are not executed for regression tests, as the libc on
       my system is at once less forgiving (segfaults on mode NULL) and more
       forgiving (accepts undefined modes).
    */
    remove(testfile);
#ifdef _PDCLIB_C_VERSION
    EXPECT_EQ(NULL, fopen(NULL, NULL));
#endif
    EXPECT_EQ(NULL, fopen(NULL, "w"));
#ifdef _PDCLIB_C_VERSION
    EXPECT_EQ(NULL, fopen("", NULL));
#endif
    EXPECT_EQ(NULL, fopen("", "w"));
    EXPECT_EQ(NULL, fopen("foo", ""));
#ifdef _PDCLIB_C_VERSION
    /* Undefined modes */
    EXPECT_EQ(NULL, fopen(testfile, "wq"));
    EXPECT_EQ(NULL, fopen(testfile, "wr"));
#endif

    {
        FILE* fh = fopen(testfile, "w");
        ASSERT_NE(nullptr, fh);
        EXPECT_EQ(0, fclose(fh));
        EXPECT_EQ(0, remove(testfile));
    }
}

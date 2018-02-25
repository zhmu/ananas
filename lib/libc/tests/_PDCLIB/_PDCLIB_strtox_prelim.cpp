#include <gtest/gtest.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>

#ifdef _PDCLIB_C_VERSION
TEST(_PDCLIB, _PDCLIB_strtox_prelim)
{
{
    char test1[] = "  123";
    char test2[] = "\t+0123";
    char test3[] = "\v-0x123";

    {
        int base = 0;
        char sign = '\0';
        EXPECT_EQ(&test1[2], _PDCLIB_strtox_prelim(test1, &sign, &base));
        EXPECT_EQ('+', sign);
        EXPECT_EQ(10, base);
    }
    {
        int base = 0;
        char sign = '\0';
        EXPECT_EQ(&test2[3], _PDCLIB_strtox_prelim(test2, &sign, &base));
        EXPECT_EQ('+', sign);
        EXPECT_EQ(8, base);
    }
    {
        int base = 0;
        char sign = '\0';
        EXPECT_EQ(&test3[4], _PDCLIB_strtox_prelim(test3, &sign, &base));
        EXPECT_EQ('-', sign);
        EXPECT_EQ(16, base);
    }
    {
        int base = 10;
        char sign = '\0';
        TESTCASE( _PDCLIB_strtox_prelim( test3, &sign, &base ) == &test3[2] );
        EXPECT_EQ('-', sign);
        EXPECT_EQ(10, base);
    }
    {
        int base = 1;
        char sign = '\0';
        EXPECT_EQ(NULL, _PDCLIB_strtox_prelim(test3, &sign, &base));
    }
    {
        int base = 37;
        char sign = '\0';
        EXPECT_EQ(NULL, _PDCLIB_strtox_prelim(test3, &sign, &base));
     }
}
#endif

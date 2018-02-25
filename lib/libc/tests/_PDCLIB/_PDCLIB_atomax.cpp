#include <gtest/gtest.h>
#include <string.h>
#include <ctype.h>

#ifdef _PDCLIB_C_VERSION
TEST(_PDCLIB, _PDCLIB_atomax)
{
    /* basic functionality */
    EXPECT_EQ(123, _PDCLIB_atomax("123"));
    /* testing skipping of leading whitespace and trailing garbage */
    EXPECT_EQ(123, _PDCLIB_atomax(" \n\v\t\f123xyz"));
}
#endif

#include <gtest/gtest.h>
#include <string.h>

#ifdef _PDCLIB_C_VERSION
TEST(_PDCLIB, _PDCLIB_digits)
{
    EXPECT_EQ(0, strcmp( _PDCLIB_digits, "0123456789abcdefghijklmnopqrstuvwxyz"));
    EXPECT_EQ(0, strcmp( _PDCLIB_Xdigits, "0123456789ABCDEF"));
}
#endif

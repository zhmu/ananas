#include <gtest/gtest.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#ifdef _PDCLIB_C_VERSION
TEST(_PDCLIB, _PDCLIB_strtox_prelim)
{
    const char* p;
    char test[] = "123_";
    char fail[] = "xxx";
    char sign = '-';
    /* basic functionality */
    p = test;
    errno = 0;
    EXPECT_EQ(123, _PDCLIB_strtox_main(&p, 10u, (uintmax_t)999, (uintmax_t)12, 3, &sign));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(&test[3], p);
    /* proper functioning to smaller base */
    p = test;
    EXPECT_EQ(0123, _PDCLIB_strtox_main(&p, 8u, (uintmax_t)999, (uintmax_t)12, 3, &sign));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(&test[3], p);
    /* overflowing subject sequence must still return proper endptr */
    p = test;
    EXPECT_EQ(999, _PDCLIB_strtox_main(&p, 4u, (uintmax_t)999, (uintmax_t)1, 2, &sign));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(&test[3], p);
    EXPECT_EQ('+', sign);
    /* testing conversion failure */
    errno = 0;
    p = fail;
    sign = '-';
    EXPECT_EQ(0, _PDCLIB_strtox_main(&p, 10u, (uintmax_t)999, (uintmax_t)99, 8, &sign));
    EXPECT_EQ(NULL, p);
}
#endif

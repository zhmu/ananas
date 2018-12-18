#include <gtest/gtest.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

TEST(stdlib, strtoul)
{
    char* endptr;
    /* this, to base 36, overflows even a 256 bit integer */
    const char overflow[] = "-ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ_";
    /* tricky border case */
    const char tricky[] = "+0xz";
    errno = 0;
    /* basic functionality */
    EXPECT_EQ(123, strtoul("123", NULL, 10));
    /* proper detecting of default base 10 */
    EXPECT_EQ(456, strtoul("456", NULL, 0));
    /* proper functioning to smaller base */
    EXPECT_EQ(12, strtoul("14", NULL, 8));
    /* proper autodetecting of octal */
    EXPECT_EQ(14, strtoul("016", NULL, 0));
    /* proper autodetecting of hexadecimal, lowercase 'x' */
    EXPECT_EQ(255, strtoul("0xFF", NULL, 0));
    /* proper autodetecting of hexadecimal, uppercase 'X' */
    EXPECT_EQ(161, strtoul("0Xa1", NULL, 0));
    /* proper handling of border case: 0x followed by non-hexdigit */
    EXPECT_EQ(0, strtoul(tricky, &endptr, 0));
    EXPECT_EQ(tricky + 2, endptr);
    /* proper handling of border case: 0 followed by non-octdigit */
    EXPECT_EQ(0, strtoul(tricky, &endptr, 8));
    EXPECT_EQ(tricky + 2, endptr);
    /* errno should still be 0 */
    EXPECT_EQ(0, errno);
    /* overflowing subject sequence must still return proper endptr */
    EXPECT_EQ(ULONG_MAX, strtoul(overflow, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* same for positive */
    errno = 0;
    EXPECT_EQ(ULONG_MAX, strtoul(overflow + 1, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* testing skipping of leading whitespace */
    EXPECT_EQ(789, strtoul(" \n\v\t\f789", NULL, 0));
    /* testing conversion failure */
    EXPECT_EQ(0, strtoul(overflow, &endptr, 10));
    EXPECT_EQ(overflow, endptr);
    endptr = NULL;
    EXPECT_EQ(0, strtoul(overflow, &endptr, 0));
    EXPECT_EQ(overflow, endptr);
    /* TODO: These tests assume two-complement, but conversion should work */
    /* for one-complement and signed magnitude just as well. Anyone having */
    /* a platform to test this on?                                         */
    errno = 0;
/* long -> 32 bit */
#if ULONG_MAX >> 31 == 1
    /* testing "even" overflow, i.e. base is power of two */
    EXPECT_EQ(ULONG_MAX, strtoul("4294967295", NULL, 0));
    EXPECT_EQ(0, errno);
    errno = 0;
    EXPECT_EQ(ULONG_MAX, strtoul("4294967296", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    /* TODO: test "odd" overflow, i.e. base is not power of two */
/* long -> 64 bit */
#elif ULONG_MAX >> 63 == 1
    /* testing "even" overflow, i.e. base is power of two */
    EXPECT_EQ(ULONG_MAX, strtoul("18446744073709551615", NULL, 0));
    EXPECT_EQ(0, errno);
    errno = 0;
    EXPECT_EQ(ULONG_MAX, strtoul("18446744073709551616", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    /* TODO: test "odd" overflow, i.e. base is not power of two */
#else
#error Unsupported width of 'long' (neither 32 nor 64 bit).
#endif
}

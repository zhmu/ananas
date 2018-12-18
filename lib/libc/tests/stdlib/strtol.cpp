#include <gtest/gtest.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

TEST(stdlib, strtol)
{
    char* endptr;
    /* this, to base 36, overflows even a 256 bit integer */
    const char overflow[] = "-ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ_";
    /* tricky border case */
    const char tricky[] = "+0xz";
    errno = 0;
    /* basic functionality */
    EXPECT_EQ(123, strtol("123", NULL, 10));
    /* proper detecting of default base 10 */
    EXPECT_EQ(456, strtol("456", NULL, 0));
    /* proper functioning to smaller base */
    EXPECT_EQ(12, strtol("14", NULL, 8));
    /* proper autodetecting of octal */
    EXPECT_EQ(14, strtol("016", NULL, 0));
    /* proper autodetecting of hexadecimal, lowercase 'x' */
    EXPECT_EQ(255, strtol("0xFF", NULL, 0));
    /* proper autodetecting of hexadecimal, uppercase 'X' */
    EXPECT_EQ(161, strtol("0Xa1", NULL, 0));
    /* proper handling of border case: 0x followed by non-hexdigit */
    EXPECT_EQ(0, strtol(tricky, &endptr, 0));
    EXPECT_EQ(tricky + 2, endptr);
    /* proper handling of border case: 0 followed by non-octdigit */
    EXPECT_EQ(0, strtol(tricky, &endptr, 8));
    EXPECT_EQ(tricky + 2, endptr);
    /* errno should still be 0 */
    EXPECT_EQ(0, errno);
    /* overflowing subject sequence must still return proper endptr */
    EXPECT_EQ(LONG_MIN, strtol(overflow, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* same for positive */
    errno = 0;
    EXPECT_EQ(LONG_MAX, strtol(overflow + 1, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* testing skipping of leading whitespace */
    EXPECT_EQ(789, strtol("\n\v\t\f789", NULL, 0));
    /* testing conversion failure */
    EXPECT_EQ(0, strtol(overflow, &endptr, 10));
    EXPECT_EQ(overflow, endptr);
    endptr = NULL;
    EXPECT_EQ(0, strtol(overflow, &endptr, 0));
    EXPECT_EQ(overflow, endptr);
    /* TODO: These tests assume two-complement, but conversion should work */
    /* for one-complement and signed magnitude just as well. Anyone having */
    /* a platform to test this on?                                         */
    errno = 0;
#if LONG_MAX >> 30 == 1
    /* testing "even" overflow, i.e. base is power of two */
    EXPECT_EQ(0x7fffffff, strtol("2147483647", NULL, 0));
    EXPECT_EQ(0, errno);
    errno = 0;
    EXPECT_EQ(LONG_MAX, strtol("2147483648", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    errno = 0;
    EXPECT_EQ((long)0x80000001, strtol("-2147483647", NULL, 0));
    EXPECT_EQ(0, errno);
    errno = 0;
    EXPECT_EQ(LONG_MIN, strtol("-2147483648", NULL, 0));
    EXPECT_EQ(0, errno);
    errno = 0;
    EXPECT_EQ(LONG_MIN, strtol("-2147483649", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    /* TODO: test "odd" overflow, i.e. base is not power of two */
#elif LONG_MAX >> 62 == 1
    /* testing "even" overflow, i.e. base is power of two */
    EXPECT_EQ(0x7fffffffffffffff, strtol("9223372036854775807", NULL, 0));
    EXPECT_EQ(0, errno);
    errno = 0;
    EXPECT_EQ(LONG_MAX, strtol("9223372036854775808", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    errno = 0;
    EXPECT_EQ((long)0x8000000000000001, strtol("-9223372036854775807", NULL, 0));
    EXPECT_EQ(0, errno);
    errno = 0;
    EXPECT_EQ(LONG_MIN, strtol("-9223372036854775808", NULL, 0));
    EXPECT_EQ(0, errno);
    errno = 0;
    EXPECT_EQ(LONG_MIN, strtol("-9223372036854775809", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    /* TODO: test "odd" overflow, i.e. base is not power of two */
#else
#error Unsupported width of 'long' (neither 32 nor 64 bit).
#endif
}

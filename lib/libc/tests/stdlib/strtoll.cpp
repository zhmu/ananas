#include <gtest/gtest.h>
#include <stdlib.h>
#include <limits.h>

TEST(stdlib, strtoll)
{
    char * endptr;
    /* this, to base 36, overflows even a 256 bit integer */
    const char overflow[] = "-ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ_";
    /* tricky border case */
    const char tricky[] = "+0xz";
    errno = 0;
    /* basic functionality */
    EXPECT_EQ(123, strtoll("123", NULL, 10));
    /* proper detecting of default base 10 */
    EXPECT_EQ(456, strtoll("456", NULL, 0));
    /* proper functioning to smaller base */
    EXPECT_EQ(12, strtoll( "14", NULL, 8));
    /* proper autodetecting of octal */
    EXPECT_EQ(14, strtoll("016", NULL, 0));
    /* proper autodetecting of hexadecimal, lowercase 'x' */
    EXPECT_EQ(255, strtoll("0xFF", NULL, 0));
    /* proper autodetecting of hexadecimal, uppercase 'X' */
    EXPECT_EQ(161, strtoll("0Xa1", NULL, 0));
    /* proper handling of border case: 0x followed by non-hexdigit */
    EXPECT_EQ(0, strtoll(tricky, &endptr, 0));
    EXPECT_EQ(tricky + 2, endptr);
    /* proper handling of border case: 0 followed by non-octdigit */
    EXPECT_EQ(0, strtoll(tricky, &endptr, 8));
    EXPECT_EQ(tricky + 2, endptr);
    /* errno should still be 0 */
    EXPECT_EQ(0, errno);
    /* overflowing subject sequence must still return proper endptr */
    EXPECT_EQ(LLONG_MIN, strtoll(overflow, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* same for positive */
    errno = 0;
    EXPECT_EQ(LLONG_MAX, strtoll(overflow + 1, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* testing skipping of leading whitespace */
    EXPECT_EQ(789, strtoll(" \n\v\t\f789", NULL, 0));
    /* testing conversion failure */
    EXPECT_EQ(0, strtoll(overflow, &endptr, 10));
    EXPECT_EQ(overflow, endptr);
    endptr = NULL;
    EXPECT_EQ(0, strtoll(overflow, &endptr, 0));
    EXPECT_EQ(overflow, endptr);
    /* TODO: These tests assume two-complement, but conversion should work */
    /* for one-complement and signed magnitude just as well. Anyone having */
    /* a platform to test this on?                                         */
    errno = 0;
#if LLONG_MAX >> 62 == 1
    /* testing "even" overflow, i.e. base is power of two */
    EXPECT_EQ(0x7fffffffffffffff, strtoll("9223372036854775807", NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(LLONG_MAX, strtoll("9223372036854775808", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    errno = 0;
    EXPECT_EQ((long long)0x8000000000000001, strtoll( "-9223372036854775807", NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(LLONG_MIN, strtoll("-9223372036854775808", NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(LLONG_MIN, strtoll("-9223372036854775809", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    /* TODO: test "odd" overflow, i.e. base is not power of two */
#elif LLONG_MAX >> 126 == 1
    /* testing "even" overflow, i.e. base is power of two */
    EXPECT_EQ(0x7fffffffffffffffffffffffffffffff, strtoll("170141183460469231731687303715884105728", NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(LLONG_MAX, strtoll("170141183460469231731687303715884105729", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    errno = 0;
    EXPECT_EQ(-0x80000000000000000000000000000001, strtoll("-170141183460469231731687303715884105728", NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(LLONG_MIN, strtoll("-170141183460469231731687303715884105729", NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(LLONG_MIN, strtoll("-170141183460469231731687303715884105730", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    /* TODO: test "odd" overflow, i.e. base is not power of two */
#else
#error Unsupported width of 'long' (neither 32 nor 64 bit).
#endif
}

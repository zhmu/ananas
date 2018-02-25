#include <gtest/gtest.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

TEST(stdlib, strtoull)
{
    char * endptr;
    /* this, to base 36, overflows even a 256 bit integer */
    const char overflow[] = "-ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ_";
    /* tricky border case */
    const char tricky[] = "+0xz";
    errno = 0;
    /* basic functionality */
    EXPECT_EQ(123, strtoull("123", NULL, 10));
    /* proper detecting of default base 10 */
    EXPECT_EQ(456, strtoull("456", NULL, 0));
    /* proper functioning to smaller base */
    EXPECT_EQ(12, strtoull( "14", NULL, 8));
    /* proper autodetecting of octal */
    EXPECT_EQ(14, strtoull("016", NULL, 0));
    /* proper autodetecting of hexadecimal, lowercase 'x' */
    EXPECT_EQ(255, strtoull("0xFF", NULL, 0));
    /* proper autodetecting of hexadecimal, uppercase 'X' */
    EXPECT_EQ(161, strtoull("0Xa1", NULL, 0));
    /* proper handling of border case: 0x followed by non-hexdigit */
    EXPECT_EQ(0, strtoull(tricky, &endptr, 0));
    EXPECT_EQ(tricky + 2, endptr);
    /* proper handling of border case: 0 followed by non-octdigit */
    EXPECT_EQ(0, strtoull(tricky, &endptr, 8));
    EXPECT_EQ(tricky + 2, endptr);
    /* errno should still be 0 */
    EXPECT_EQ(0, errno);
    /* overflowing subject sequence must still return proper endptr */
    EXPECT_EQ(ULLONG_MAX, strtoull(overflow, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* same for positive */
    errno = 0;
    EXPECT_EQ(ULLONG_MAX, strtoull(overflow + 1, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* testing skipping of leading whitespace */
    EXPECT_EQ(789, strtoull(" \n\v\t\f789", NULL, 0));
    /* testing conversion failure */
    EXPECT_EQ(0, strtoull(overflow, &endptr, 10));
    EXPECT_EQ(overflow, endptr);
    endptr = NULL;
    EXPECT_EQ(0, strtoull(overflow, &endptr, 0));
    EXPECT_EQ(overflow, endptr);
    errno = 0;
/* long long -> 64 bit */
#if ULLONG_MAX >> 63 == 1
    /* testing "even" overflow, i.e. base is power of two */
    EXPECT_EQ(ULLONG_MAX, strtoull("18446744073709551615", NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(ULLONG_MAX, strtoull("18446744073709551616", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    /* TODO: test "odd" overflow, i.e. base is not power of two */
/* long long -> 128 bit */
#elif ULLONG_MAX >> 127 == 1
    /* testing "even" overflow, i.e. base is power of two */
    EXPECT_EQ(ULLONG_MAX, strtoull("340282366920938463463374607431768211455", NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(ULLONG_MAX, strtoull("340282366920938463463374607431768211456", NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    /* TODO: test "odd" overflow, i.e. base is not power of two */
#else
#error Unsupported width of 'long' (neither 32 nor 64 bit).
#endif
}

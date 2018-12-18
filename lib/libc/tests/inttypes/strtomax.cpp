#include <gtest/gtest.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>

#if UINTMAX_MAX >> 127 == 1

#define MAX_S_STRING_DEC "170141183460469231731687303715884105728"
#define MAX_S_STRING_DEC_PLUS_1 "170141183460469231731687303715884105729"
#define MAX_S_STRING_HEX "0x7fffffffffffffffffffffffffffffff"
#define MAX_S_STRING_HEX_PLUS_1 "0x80000000000000000000000000000000"
#define MIN_S_STRING_DEC "-170141183460469231731687303715884105729"
#define MIN_S_STRING_DEC_MINUS_1 "-170141183460469231731687303715884105730"
#define MIN_S_STRING_HEX "-0x80000000000000000000000000000000"
#define MIN_S_STRING_HEX_MINUS_1 "-0x80000000000000000000000000000001"

#define MAX_U_STRING_DEC "340282366920938463463374607431768211455"
#define MAX_U_STRING_DEC_PLUS_1 "340282366920938463463374607431768211456"
#define MAX_U_STRING_HEX "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define MAX_U_STRING_HEX_PLUS_1 "0x100000000000000000000000000000000"

#elif UINTMAX_MAX >> 63 == 1

#define MAX_S_STRING_DEC "9223372036854775807"
#define MAX_S_STRING_DEC_PLUS_1 "9223372036854775808"
#define MIN_S_STRING_DEC "-9223372036854775808"
#define MIN_S_STRING_DEC_MINUS_1 "-9223372036854775809"
#define MAX_S_STRING_HEX "0x7fffffffffffffff"
#define MAX_S_STRING_HEX_PLUS_1 "0x8000000000000000"
#define MIN_S_STRING_HEX "-0x8000000000000000"
#define MIN_S_STRING_HEX_MINUS_1 "-0x8000000000000001"

#define MAX_U_STRING_DEC "18446744073709551615"
#define MAX_U_STRING_DEC_PLUS_1 "18446744073709551616"
#define MAX_U_STRING_HEX "0xFFFFFFFFFFFFFFFF"
#define MAX_U_STRING_HEX_PLUS_1 "0x10000000000000000"
#else
#error Unsupported width of 'uintmax_t' (neither 64 nor 128 bit).
#endif

template<typename Func, typename Range>
void TestStrTo(Func conv, const Range&)
{
    char* endptr;
    /* this, to base 36, overflows even a 256 bit integer */
    char overflow[] = "-ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ_";
    /* tricky border case */
    char tricky[] = "+0xz";
    errno = 0;
    /* basic functionality */
    EXPECT_EQ(123, conv("123", NULL, 10));
    /* proper detecting of default base 10 */
    EXPECT_EQ(456, conv("456", NULL, 0));
    /* proper functioning to smaller base */
    EXPECT_EQ(12, conv("14", NULL, 8));
    /* proper autodetecting of octal */
    EXPECT_EQ(14, conv("016", NULL, 0));
    /* proper autodetecting of hexadecimal, lowercase 'x' */
    EXPECT_EQ(255, conv("0xFF", NULL, 0));
    /* proper autodetecting of hexadecimal, uppercase 'X' */
    EXPECT_EQ(161, conv("0Xa1", NULL, 0));
    /* proper handling of border case: 0x followed by non-hexdigit */
    EXPECT_EQ(0, conv(tricky, &endptr, 0));
    EXPECT_EQ(tricky + 2, endptr);
    /* proper handling of border case: 0 followed by non-octdigit */
    EXPECT_EQ(0, conv(tricky, &endptr, 8));
    EXPECT_EQ(tricky + 2, endptr);
    /* errno should still be 0 */
    EXPECT_EQ(0, errno);
    /* overflowing subject sequence must still return proper endptr */
    EXPECT_EQ(Range::Overflow(), conv(overflow, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* same for positive */
    errno = 0;
    EXPECT_EQ(Range::Max(), conv(overflow + 1, &endptr, 36));
    EXPECT_EQ(ERANGE, errno);
    EXPECT_EQ(53, endptr - overflow);
    /* testing skipping of leading whitespace */
    EXPECT_EQ(789, conv(" \n\v\t\f789", NULL, 0));
    /* testing conversion failure */
    EXPECT_EQ(0, conv(overflow, &endptr, 10));
    EXPECT_EQ(overflow, endptr);
    endptr = NULL;
    EXPECT_EQ(0, conv(overflow, &endptr, 0));
    EXPECT_EQ(overflow, endptr);
    errno = 0;
    /* testing "odd" overflow, i.e. base is not power of two */
    EXPECT_EQ(Range::Max(), conv(Range::MaxStringDec(), NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(Range::Max(), conv(Range::MaxStringDecPlus1(), NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    if constexpr (Range::IsSigned()) {
        errno = 0;
        EXPECT_EQ(Range::Min(), strtoimax(Range::MinStringDec(), NULL, 0));
        EXPECT_EQ(0, errno);
        EXPECT_EQ(Range::Min(), strtoimax(Range::MinStringDecMinus1(), NULL, 0));
        EXPECT_EQ(ERANGE, errno);
    }
    /* testing "even" overflow, i.e. base is power of two */
    errno = 0;
    EXPECT_EQ(Range::Max(), conv(Range::MaxStringHex(), NULL, 0));
    EXPECT_EQ(0, errno);
    EXPECT_EQ(Range::Max(), conv(Range::MaxStringHexPlus1(), NULL, 0));
    EXPECT_EQ(ERANGE, errno);
    if constexpr (Range::IsSigned()) {
        errno = 0;
        EXPECT_EQ(Range::Min(), strtoimax(Range::MinStringHex(), NULL, 0));
        EXPECT_EQ(0, errno);
        EXPECT_EQ(Range::Min(), strtoimax(Range::MinStringHexMinus1(), NULL, 0));
        EXPECT_EQ(ERANGE, errno);
    }
}

TEST(inttypes, strtoimax)
{
    struct IMaxRanges {
        static constexpr intmax_t Min() { return INTMAX_MIN; }
        static constexpr intmax_t Max() { return INTMAX_MAX; }
        static constexpr intmax_t Overflow() { return INTMAX_MIN; }

        static constexpr const char* MaxStringHex() { return MAX_S_STRING_HEX; }

        static constexpr const char* MaxStringHexPlus1() { return MAX_S_STRING_HEX_PLUS_1; }

        static constexpr const char* MaxStringDec() { return MAX_S_STRING_DEC; }

        static constexpr const char* MaxStringDecPlus1() { return MAX_S_STRING_DEC_PLUS_1; }

        static constexpr const char* MinStringDec() { return MIN_S_STRING_DEC; }

        static constexpr const char* MinStringDecMinus1() { return MIN_S_STRING_DEC_MINUS_1; }

        static constexpr const char* MinStringHex() { return MIN_S_STRING_HEX; }

        static constexpr const char* MinStringHexMinus1() { return MIN_S_STRING_HEX_MINUS_1; }

        static constexpr bool IsSigned() { return true; }
    };

    TestStrTo(strtoimax, IMaxRanges());
}

TEST(inttypes, strtoumax)
{
    struct UIMaxRanges {
        static constexpr uintmax_t Max() { return UINTMAX_MAX; }
        static constexpr uintmax_t Overflow() { return UINTMAX_MAX; }

        static constexpr const char* MaxStringHex() { return MAX_U_STRING_HEX; }
        static constexpr const char* MaxStringHexPlus1() { return MAX_U_STRING_HEX_PLUS_1; }
        static constexpr const char* MaxStringDec() { return MAX_U_STRING_DEC; }
        static constexpr const char* MaxStringDecPlus1() { return MAX_U_STRING_DEC_PLUS_1; }

        static constexpr bool IsSigned() { return false; }
    };

    TestStrTo(strtoumax, UIMaxRanges());
}

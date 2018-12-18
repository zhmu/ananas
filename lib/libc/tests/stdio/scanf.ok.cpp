#include <gtest/gtest.h>
#include <stdio.h>
#include <limits.h>

#if INT_MAX >> 15 == 1

#define UINT_DIG 5
#define INT_DIG 5
#define INT_DIG_LESS1 "4"
#define INT_DIG_PLUS1 "6"
#define INT_DIG_PLUS2 "7"
#define INT_HEXDIG "FFF"
#define INT_hexdig "fff"
#define INT_OCTDIG "177777"
#define INT_MAX_DEZ_STR "32767"
#define INT_MIN_DEZ_STR "32768"
#define UINT_MAX_DEZ_STR "65535"
#define INT_MAX_OCT_STR
#define INT_MIN_OCT_STR
#define UINT_MAX_OCT_STR
#define INT_MAX_HEX_STR
#define INT_MIN_HEX_STR
#define UINT_MAX_HEX_STR

#elif UINT_MAX >> 31 == 1

#define UINT_DIG 10
#define INT_DIG 10
#define INT_DIG_LESS1 "9"
#define INT_DIG_PLUS1 "11"
#define INT_DIG_PLUS2 "12"
#define INT_HEXDIG "FFFFFFF"
#define INT_hexdig "fffffff"
#define INT_OCTDIG "37777777777"
#define INT_MAX_DEZ_STR "2147483647"
#define INT_MIN_DEZ_STR "2147483648"
#define UINT_MAX_DEZ_STR "4294967295"
#define INT_MAX_OCT_STR
#define INT_MIN_OCT_STR
#define UINT_MAX_OCT_STR
#define INT_MAX_HEX_STR
#define INT_MIN_HEX_STR
#define UINT_MAX_HEX_STR

#elif UINT_MAX >> 63 == 1

#define UINT_DIG 20
#define INT_DIG 19
#define INT_DIG_LESS1 "18"
#define INT_DIG_PLUS1 "20"
#define INT_DIG_PLUS2 "21"
#define INT_HEXDIG "FFFFFFFFFFFFFFF"
#define INT_hexdig "fffffffffffffff"
#define INT_OCTDIG "1777777777777777777777"
#define INT_MAX_DEZ_STR "9223372036854775807"
#define INT_MIN_DEZ_STR "9223372036854775808"
#define UINT_MAX_DEZ_STR "18446744073709551615"
#define INT_MAX_OCT_STR
#define INT_MIN_OCT_STR
#define UINT_MAX_OCT_STR
#define INT_MAX_HEX_STR
#define INT_MIN_HEX_STR
#define UINT_MAX_HEX_STR

#else

#error Unsupported width of 'int' (neither 16, 32, nor 64 bit).

#endif

#define GET_RESULT                                                      \
    rewind(target);                                                     \
    if ((int)fread(result_buffer, 1, actual_rc, target) != actual_rc) { \
        fprintf(stderr, "GET_RESULT failed.");                          \
    }

#define SCANF_TEST(expected_rc, input_string, ...)         \
    do {                                                   \
        int actual_rc;                                     \
        prepareSource(input_string, sizeof(input_string)); \
        actual_rc = testscanf(source, __VA_ARGS__);        \
        EXPECT_EQ(expected_rc, actual_rc);                 \
    } while (0)

#define testscanf(stream, format, ...) scanf(format, __VA_ARGS__)

#define testfile "/tmp/z"

template<typename PS>
void PerformTests(PS prepareSource)
{
    char buffer[100];
    int i;
    unsigned int u;
    int* p;
    /* basic: reading of three-char string */
    SCANF_TEST(1, "foo", "%3c", buffer);
    EXPECT_EQ(0, memcmp(buffer, "foo", 3));
#ifndef TEST_CONVERSION_ONLY
    /* %% for single % */
    SCANF_TEST(1, "%x", "%%%c", buffer);
    EXPECT_EQ('x', buffer[0]);
    /* * to skip assignment */
    SCANF_TEST(1, "3xfoo", "%*dx%3c", buffer);
    EXPECT_EQ(0, memcmp(buffer, "foo", 3));
#endif
    /* domain testing on 'int' type */
    SCANF_TEST(1, "-" INT_MIN_DEZ_STR, "%d", &i);
    EXPECT_EQ(INT_MIN, i);
    SCANF_TEST(1, INT_MAX_DEZ_STR, "%d", &i);
    EXPECT_EQ(INT_MAX, i);
    SCANF_TEST(1, "-1", "%d", &i);
    EXPECT_EQ(-1, i);
    SCANF_TEST(1, "0", "%d", &i);
    EXPECT_EQ(0, i);
    SCANF_TEST(1, "1", "%d", &i);
    EXPECT_EQ(1, i);
    SCANF_TEST(1, "-" INT_MIN_DEZ_STR, "%i", &i);
    EXPECT_EQ(INT_MIN, i);
    SCANF_TEST(1, INT_MAX_DEZ_STR, "%i", &i);
    EXPECT_EQ(INT_MAX, i);
    SCANF_TEST(1, "-1", "%i", &i);
    EXPECT_EQ(-1, i);
    SCANF_TEST(1, "0", "%i", &i);
    EXPECT_EQ(0, i);
    SCANF_TEST(1, "1", "%i", &i);
    EXPECT_EQ(1, i);
    SCANF_TEST(1, "0x7" INT_HEXDIG, "%i", &i);
    EXPECT_EQ(INT_MAX, i);
    SCANF_TEST(1, "0x0", "%i", &i);
    EXPECT_EQ(0, i);
#ifndef TEST_CONVERSION_ONLY
    SCANF_TEST(1, "00", "%i%n", &i, &u);
    EXPECT_EQ(0, i);
    EXPECT_EQ(2, u);
#endif
    /* domain testing on 'unsigned int' type */
    SCANF_TEST(1, UINT_MAX_DEZ_STR, "%u", &u);
    EXPECT_EQ(UINT_MAX, u);
    SCANF_TEST(1, "0", "%u", &u);
    EXPECT_EQ(0, u);
    SCANF_TEST(1, "f" INT_HEXDIG, "%x", &u);
    EXPECT_EQ(UINT_MAX, u);
    SCANF_TEST(1, "7" INT_HEXDIG, "%x", &u);
    EXPECT_EQ(INT_MAX, u);
    SCANF_TEST(1, "0", "%o", &u);
    EXPECT_EQ(0, u);
    SCANF_TEST(1, INT_OCTDIG, "%o", &u);
    EXPECT_EQ(UINT_MAX, u);
    /* testing %c */
    memset(buffer, '\0', 100);
    SCANF_TEST(1, "x", "%c", buffer);
    EXPECT_EQ(0, memcmp(buffer, "x\0", 2));
    /* testing %s */
    memset(buffer, '\0', 100);
    SCANF_TEST(1, "foo bar", "%s", buffer);
    EXPECT_EQ(0, memcmp(buffer, "foo\0", 4));
#ifndef TEST_CONVERSION_ONLY
    SCANF_TEST(2, "foo bar  baz", "%s %s %n", buffer, buffer + 4, &u);
    EXPECT_EQ(9, u);
    EXPECT_EQ(0, memcmp(buffer, "foo\0bar\0", 8));
#endif
    /* testing %[ */
    SCANF_TEST(1, "abcdefg", "%[cba]", buffer);
    EXPECT_EQ(0, memcmp(buffer, "abc\0", 4));
    /* testing %p */
    p = NULL;
    sprintf(buffer, "%p", p);
    p = &i;
    SCANF_TEST(1, buffer, "%p", &p);
    EXPECT_EQ(nullptr, p);
    p = &i;
    sprintf(buffer, "%p", p);
    p = NULL;
    SCANF_TEST(1, buffer, "%p", &p);
    EXPECT_EQ(&i, p);
}

TEST(stdio, scanf)
{
    FILE* source = freopen(testfile, "wb+", stdin);
    ASSERT_NE(nullptr, source);

    PerformTests([&](const char* input_string, size_t input_len) {
        rewind(source);
        fwrite(input_string, 1, input_len, source);
        rewind(source);
    });
}

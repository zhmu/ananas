#include <gtest/gtest.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>

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

#if 0
#define SCANF_TEST(expected_rc, input_string, ...)         \
    do {                                                   \
        int actual_rc;                                     \
        prepareSource(input_string, sizeof(input_string)); \
        actual_rc = testscanf(source, __VA_ARGS__);        \
        EXPECT_EQ(expected_rc, actual_rc);                 \
    } while (0)
#endif

#define testscanf(stream, format, ...) scanf(format, __VA_ARGS__)

#define testfile "/tmp/z"

#if 0
#define SCANF_TEST(expected_rc, input_string, ...)  \
    do {                                            \
        int input_len = strlen(input_string) + 1;   \
        rewind(source);                             \
        fwrite(input_string, 1, input_len, source); \
        rewind(source);                             \
        int actual_rc = scanf(__VA_ARGS__);         \
        EXPECT_EQ(expected_rc, actual_rc);          \
    } while (0)
#endif

template<typename P, typename T>
void PerformTests(P prepare, T test)
{
    char buffer[100];
    int i;
    unsigned int u;
    int* p;

    auto SCANF_TEST = [&](int expected_rc, const char* input_string, auto... args) {
        prepare(input_string);
        int actual_rc = test(args...);
        EXPECT_EQ(expected_rc, actual_rc);
    };

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
    int fd_stdin = dup(STDIN_FILENO);
    ASSERT_GT(fd_stdin, -1);

    FILE* source = freopen(testfile, "wb+", stdin);
    ASSERT_NE(nullptr, source);

    PerformTests(
        [&](const char* input_string) {
            // prepare - fileio
            int input_len = strlen(input_string) + 1;
            rewind(source);
            fwrite(input_string, 1, input_len, source);
            rewind(source);
        },
        [&](auto... args) {
            // test
            return scanf(args...);
        });

    fclose(source);

    stdin = fdopen(fd_stdin, "w");
}

TEST(stdio, sscanf)
{
    char source[100];

    PerformTests(
        [&](const char* input_string) {
            // prepare - string
            memcpy(source, input_string, strlen(input_string) + 1);
        },
        [&](auto... args) {
            // test
            return sscanf(source, args...);
        });
}

static int vsscanf_wrapper(const char* input, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsscanf(input, format, ap);
    va_end(ap);
    return result;
}

TEST(stdio, vsscanf)
{
    char source[100];

    PerformTests(
        [&](const char* input_string) {
            // prepare - string
            memcpy(source, input_string, strlen(input_string) + 1);
        },
        [&](auto... args) {
            // test
            return vsscanf_wrapper(source, args...);
        });
}

TEST(stdio, fscanf)
{
    FILE* source = tmpfile();
    ASSERT_NE(nullptr, source);

    PerformTests(
        [&](const char* input_string) {
            // prepare - fileio
            int input_len = strlen(input_string) + 1;
            rewind(source);
            fwrite(input_string, 1, input_len, source);
            rewind(source);
        },
        [&](auto... args) {
            // test
            return fscanf(source, args...);
        });

    fclose(source);
}

static int vscanf_wrapper(FILE* stream, const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    int i = vscanf(format, arg);
    va_end(arg);
    return i;
}

TEST(stdio, vscanf)
{
    int fd_stdin = dup(STDIN_FILENO);
    ASSERT_GT(fd_stdin, -1);

    FILE* source = freopen(testfile, "wb+", stdin);
    ASSERT_NE(nullptr, source);

    PerformTests(
        [&](const char* input_string) {
            // prepare - fileio
            int input_len = strlen(input_string) + 1;
            rewind(source);
            fwrite(input_string, 1, input_len, source);
            rewind(source);
        },
        [&](auto... args) { return vscanf_wrapper(source, args...); });

    fclose(source);
    remove(testfile);

    stdin = fdopen(fd_stdin, "w");
}

static int vfscanf_wrapper(FILE* stream, const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    int i = vfscanf(stream, format, arg);
    va_end(arg);
    return i;
}

TEST(stdio, vfscanf)
{
    FILE* source = tmpfile();
    ASSERT_NE(nullptr, source);

    PerformTests(
        [&](const char* input_string) {
            // prepare - fileio
            int input_len = strlen(input_string) + 1;
            rewind(source);
            fwrite(input_string, 1, input_len, source);
            rewind(source);
        },
        [&](auto... args) {
            // test
            return vfscanf_wrapper(source, args...);
        });

    fclose(source);
}

#ifdef _PDCLIB_C_VERSION
static int pdclib_scan_wrapper(char const* s, char const* format, ...)
{
    struct _PDCLIB_status_t status;
    status.n = 0;
    status.i = 0;
    status.s = (char*)s;
    status.stream = NULL;
    va_start(status.arg, format);
    if (*(_PDCLIB_scan(format, &status)) != '\0') {
        printf("_PDCLIB_scan() did not return end-of-specifier on '%s'.\n", format);
        ++TEST_RESULTS;
    }
    va_end(status.arg);
    return status.n;
}

TEST(stdio, pdclib_scan)
{
    char source[100];

    PerformTests(
        [&](const char* input_string) {
            // prepare - string
            memcpy(source, input_string, strlen(input_string) + 1);
        },
        [&](auto... args) {
            // test
            return pdclib_scan_wrapper(source, args...);
        });
}
#endif

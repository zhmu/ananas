#include <gtest/gtest.h>
#include <stdbool.h>
#include <uchar.h>

#ifdef _PDCLIB_C_VERSION
TEST(_PDCLIB, _PDCLIB_latin1)
{
    // Valid conversion & back
    char32_t c32out[5];

    char32_t *c32ptr = &c32out[0];
    size_t    c32rem = 5;
    char     *chrptr = (char*) &abcde[0];
    size_t    chrrem = 5;
    mbstate_t mbs = { 0 };

    EXPECT_TRUE(latin1toc32(&c32ptr, &c32rem, &chrptr, &chrrem, &mbs) );
    EXPECT_EQ(0, c32rem);
    EXPECT_EQ(0, chrrem);
    EXPECT_EQ(&c32out[5], c32ptr);
    EXPECT_EQ(&abcde[5], chrptr);
    EXPECT_EQ('a' == c32out[0]);
    EXPECT_EQ('b' == c32out[1]);
    EXPECT_EQ('c' == c32out[2]);
    EXPECT_EQ('d' == c32out[3]);
    EXPECT_EQ('e' == c32out[4]);

    char chrout[5];
    c32ptr = &c32out[0];
    c32rem = 5;
    chrptr = &chrout[0];
    chrrem = 5;

    EXPECT_TRUE(c32tolatin1(&chrptr, &chrrem, &c32ptr, &c32rem, &mbs));
    EXPECT_EQ(0, c32rem);
    EXPECT_EQ(0, chrrem);
    EXPECT_EQ(&c32out[5], c32ptr);
    EXPECT_EQ(&chrout[5], chrptr);
    EXPECT_EQ(0, memcmp(chrout, abcde, 5));

    // Invalid conversions
    char32_t baduni = 0xFFFE;
    c32ptr = &baduni;
    c32rem = 1;
    chrptr = &chrout[0];
    chrrem = 5;
    EXPECT_FALSE(c32tolatin1(&chrptr, &chrrem, &c32ptr, &c32rem, &mbs));
    EXPECT_EQ(&baduni, ac32ptr);
    EXPECT_EQ(1, c32rem);
    EXPECT_EQ(&chrout[0], chrptr);
    EXPECT_EQ(5, chrrem);
}
#endif

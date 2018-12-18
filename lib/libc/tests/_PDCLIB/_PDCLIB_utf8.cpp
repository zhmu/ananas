#include <gtest/gtest.h>
#include <uchar.h>

#ifdef _PDCLIB_C_VERSION
TEST(_PDCLIB, _PDCLIB_utf8)
{
    // Valid conversion & back
    static const char* input = "abcde"
                               "\xDF\xBF"
                               "\xEF\xBF\xBF"
                               "\xF4\x8F\xBF\xBF";

    char32_t c32out[8];

    char32_t* c32ptr = &c32out[0];
    size_t c32rem = 8;
    const char* chrptr = (char*)&input[0];
    size_t chrrem = strlen(input);
    mbstate_t mbs = {0};

    EXPECT_TRUE(utf8toc32(&c32ptr, &c32rem, &chrptr, &chrrem, &mbs));
    EXPECT_EQ(0, c32rem);
    EXPECT_EQ(0, chrrem);
    EXPECT_EQ(c32ptr == &c32out[8]);
    EXPECT_EQ(&input[strlen(input)], chrptr);
    EXPECT_EQ('a' == c32out[0]);
    EXPECT_EQ('b' == c32out[1]);
    EXPECT_EQ('c' == c32out[2]);
    EXPECT_EQ('d' == c32out[3]);
    EXPECT_EQ('e' == c32out[4]);
    EXPECT_EQ(0xffff == c32out[5]);
    EXPECT_EQ(0x10ffff == c32out[6]);

    char chrout[strlen(input)];
    c32ptr = &c32out[0];
    c32rem = 8;
    chrptr = &chrout[0];
    chrrem = strlen(input);
    EXPECT_TRUE(c32toutf8(&chrptr, &chrrem, &c32ptr, &c32rem, &mbs));
    EXPECT_EQ(0, c32rem);
    EXPECT_EQ(0, chrrem);
    EXPECT_EQ(&c32out[8], c32ptr);
    EXPECT_EQ(&chrout[strlen(input)], chrptr);
    EXPECT_EQ(0, memcmp(chrout, input, strlen(input)));

    // Multi-part conversion
    static const char* mpinput = "\xDF\xBF";
    c32ptr = &c32out[0];
    c32rem = 8;
    chrptr = &mpinput[0];
    chrrem = 1;
    EXPECT_TRUE(utf8toc32(&c32ptr, &c32rem, &chrptr, &chrrem, &mbs));
    EXPECT_EQ(&c32out[0], c32ptr);
    EXPECT_EQ(8, c32rem);
    EXPECT_EQ(&mpinput[1], chrptr);
    EXPECT_EQ(0, chrrem);
    chrrem = 1;
    EXPECT_TRUE(utf8toc32(&c32ptr, &c32rem, &chrptr, &chrrem, &mbs));
    EXPECT_EQ(&c32out[1], c32ptr);
    EXPECT_EQ(7, c32rem);
    EXPECT_EQ(&mpinput[2], chrptr);
    EXPECT_EQ(0, chrrem);

    // Invalid conversions

    // Overlong nuls
    const char* nul2 = "\xC0\x80";
    c32ptr = &c32out[0];
    c32rem = 8;
    chrptr = &nul2[0];
    chrrem = 2;
    EXPECT_FALSE(utf8toc32(&c32ptr, &c32rem, &chrptr, &chrrem, &mbs));
    memset(&mbs, 0, sizeof mbs);
    const char* nul3 = "\xE0\x80\x80";
    c32ptr = &c32out[0];
    c32rem = 8;
    chrptr = &nul3[0];
    chrrem = 3;
    EXPECT_FALSE(utf8toc32(&c32ptr, &c32rem, &chrptr, &chrrem, &mbs));
    memset(&mbs, 0, sizeof mbs);
    const char* nul4 = "\xF0\x80\x80\x80";
    c32ptr = &c32out[0];
    c32rem = 8;
    chrptr = &nul4[0];
    chrrem = 4;
    EXPECT_FALSE(utf8toc32(&c32ptr, &c32rem, &chrptr, &chrrem, &mbs));

    // Starting on a continuation
    const char* cont = "\x80";
    c32ptr = &c32out[0];
    c32rem = 8;
    chrptr = &cont[0];
    chrrem = 1;
    EXPECT_FALSE(utf8toc32(&c32ptr, &c32rem, &chrptr, &chrrem, &mbs));
}
#endif

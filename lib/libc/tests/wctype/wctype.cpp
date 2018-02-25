#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, wctype)
{
    EXPECT_EQ(0, wctype(""));
#ifdef _PDCLIB_C_VERSION
    EXPECT_EQ(0, wctype(NULL) == 0);
#endif

    EXPECT_NE(0, wctype("alpha"));
    EXPECT_NE(0, wctype("alnum"));
    EXPECT_NE(0, wctype("blank"));
    EXPECT_NE(0, wctype("cntrl"));
    EXPECT_NE(0, wctype("digit"));
    EXPECT_NE(0, wctype("graph"));
    EXPECT_NE(0, wctype("lower"));
    EXPECT_NE(0, wctype("print"));
    EXPECT_NE(0, wctype("punct"));
    EXPECT_NE(0, wctype("space"));
    EXPECT_NE(0, wctype("upper"));
    EXPECT_NE(0, wctype("xdigit"));

#ifdef _PDCLIB_C_VERSION
    EXPECT_EQ(_PDCLIB_CTYPE_ALPHA, wctype("alpha"));
    EXPECT_EQ((_PDCLIB_CTYPE_ALPHA | _PDCLIB_CTYPE_DIGIT), _wctype("alnum"));
    EXPECT_EQ(_PDCLIB_CTYPE_BLANK, wctype("blank"));
    EXPECT_EQ(_PDCLIB_CTYPE_CNTRL, wctype("cntrl"));
    EXPECT_EQ(_PDCLIB_CTYPE_DIGIT, wctype("digit"));
    EXPECT_EQ(_PDCLIB_CTYPE_GRAPH, wctype("graph"));
    EXPECT_EQ(_PDCLIB_CTYPE_LOWER, wctype("lower"));
    EXPECT_EQ((_PDCLIB_CTYPE_GRAPH | _PDCLIB_CTYPE_SPACE), wctype("print"));
    EXPECT_EQ(_PDCLIB_CTYPE_PUNCT, wctype("punct"));
    EXPECT_EQ(_PDCLIB_CTYPE_SPACE, wctype("space"));
    EXPECT_EQ(_PDCLIB_CTYPE_UPPER, wctype("upper"));
    EXPECT_EQ(_PDCLIB_CTYPE_XDIGT, wctype("xdigit"));
#endif
}

#include <gtest/gtest.h>
#include <stdio.h>

#ifdef _PDCLIB_C_VERSION
TEST(stdio, pdclib_filemode)
{
    EXPECT_EQ(_PDCLIB_FREAD, _PDCLIB_filemode( "r"));
    EXPECT_EQ(_PDCLIB_FWRITE, _PDCLIB_filemode( "w"));
    EXPECT_EQ((_PDCLIB_FAPPEND | _PDCLIB_FWRITE), _PDCLIB_filemode( "a"));
    EXPECT_EQ((_PDCLIB_FREAD | _PDCLIB_FRW), _PDCLIB_filemode( "r+"));
    EXPECT_EQ((_PDCLIB_FWRITE | _PDCLIB_FRW), _PDCLIB_filemode( "w+"));
    EXPECT_EQ((_PDCLIB_FAPPEND | _PDCLIB_FWRITE | _PDCLIB_FRW), _PDCLIB_filemode( "a+"));
    EXPECT_EQ((_PDCLIB_FREAD | _PDCLIB_FBIN), _PDCLIB_filemode( "rb"));
    EXPECT_EQ((_PDCLIB_FWRITE | _PDCLIB_FBIN), _PDCLIB_filemode( "wb"));
    EXPECT_EQ((_PDCLIB_FAPPEND | _PDCLIB_FWRITE | _PDCLIB_FBIN), _PDCLIB_filemode( "ab"));
    EXPECT_EQ((_PDCLIB_FREAD | _PDCLIB_FRW | _PDCLIB_FBIN), _PDCLIB_filemode( "r+b"));
    EXPECT_EQ((_PDCLIB_FWRITE | _PDCLIB_FRW | _PDCLIB_FBIN), _PDCLIB_filemode( "w+b"));
    EXPECT_EQ((_PDCLIB_FAPPEND | _PDCLIB_FWRITE | _PDCLIB_FRW | _PDCLIB_FBIN), _PDCLIB_filemode( "a+b"));
    EXPECT_EQ((_PDCLIB_FREAD | _PDCLIB_FRW | _PDCLIB_FBIN), _PDCLIB_filemode( "rb+"));
    EXPECT_EQ((_PDCLIB_FWRITE | _PDCLIB_FRW | _PDCLIB_FBIN), _PDCLIB_filemode( "wb+"));
    EXPECT_EQ((_PDCLIB_FAPPEND | _PDCLIB_FWRITE | _PDCLIB_FRW | _PDCLIB_FBIN), _PDCLIB_filemode( "ab+"));
    EXPECT_EQ(0, _PDCLIB_filemode("x"));
    EXPECT_EQ(0, _PDCLIB_filemode("r++"));
    EXPECT_EQ(0, _PDCLIB_filemode("wbb"));
    EXPECT_EQ(0, _PDCLIB_filemode("a+bx"));
}
#endif

#include <gtest/gtest.h>
#include <stdio.h>

TEST(stdio, ftell)
{
    /* Testing all the basic I/O functions individually would result in lots
       of duplicated code, so I took the liberty of lumping it all together
       here.
    */
    /* The following functions delegate their tests to here:
       fgetc fflush rewind fputc ungetc fseek
       flushbuffer seek fillbuffer prepread prepwrite
    */
    FILE* fh = tmpfile();
    ASSERT_NE(nullptr, fh);

    auto buffer = static_cast<char*>(malloc(4));
    EXPECT_EQ(0, setvbuf(fh, buffer, _IOLBF, 4));
    /* Testing ungetc() at offset 0 */
    rewind(fh);
#ifdef _PDCLIB_C_VERSION
    // XXX This crashes on glibc - need to investigate why
    EXPECT_EQ('x', ungetc('x', fh));
    EXPECT_EQ(-1L, ftell(fh));
    rewind(fh);
#endif
    EXPECT_EQ(0, ftell(fh));
    /* Commence "normal" tests */
    EXPECT_EQ('1', fputc('1', fh));
    EXPECT_EQ('2', fputc('2', fh));
    EXPECT_EQ('3', fputc('3', fh));
    /* Positions incrementing as expected? */
    EXPECT_EQ(3L, ftell(fh));
#ifdef _PDCLIB_C_VERSION
    EXPECT_EQ(0, fh->pos.offset);
    EXPECT_EQ(3, fh->bufidx);
#endif
    /* Buffer properly flushed when full? */
    EXPECT_EQ('4', fputc('4', fh));
#ifdef _PDCLIB_C_VERSION
    EXPECT_EQ(4, fh->pos.offset);
    EXPECT_EQ(0, fh->bufidx);
#endif
    /* fflush() resetting positions as expected? */
    EXPECT_EQ('5', fputc('5', fh));
    EXPECT_EQ(0, fflush(fh));
    EXPECT_EQ(5, ftell(fh));
#ifdef _PDCLIB_C_VERSION
    EXPECT_EQ(5, fh->pos.offset);
    EXPECT_EQ(0, fh->bufidx);
#endif
    /* rewind() resetting positions as expected? */
    rewind(fh);
    EXPECT_EQ(0, ftell(fh));
#ifdef _PDCLIB_C_VERSION
    EXPECT_EQ(0, fh->pos.offset);
    EXPECT_EQ(0, fh->bufidx);
#endif
    /* Reading back first character after rewind for basic read check */
    EXPECT_EQ('1', fgetc(fh));
    /* TODO: t.b.c. */
    EXPECT_EQ(0, fclose(fh));
}

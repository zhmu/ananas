#include <gtest/gtest.h>
#include <stdio.h>

#define BUFFERSIZE 500

#ifdef _PDCLIB_C_VERSION
// setbuf() test driver is PDCLib-specific
TEST(stdio, setbuf)
{
    /* TODO: Extend testing once setvbuf() is finished. */
    /* full buffered */
    {
        char buffer[BUFSIZ + 1];
        FILE* fh = tmpfile();
        ASSERT_NE(nullptr, fh);
        setbuf(fh, buffer);
        EXPECT_EQ(buffer, fh->buffer);
        EXPECT_EQ(BUFSIZ, fh->bufsize);
        EXPECT_EQ(_IOFBF, (fh->status & (_IOFBF | _IONBF | _IOLBF)); EXPECT_EQ(0, fclose(fh)));
    }
    /* not buffered */
    {
        FILE* fh = tmpfile();
        ASSERT_NE(nullptr, fh);
        setbuf(fh, NULL);
        EXPECT_EQ(_IOFBF, (fh->status & (_IOFBF | _IONBF | _IOLBF)));
        EXPECT_EQ(0, fclose(fh));
    }
}
#endif

#include <gtest/gtest.h>
#include <stdio.h>

#define BUFFERSIZE 500

#ifdef _PDCLIB_C_VERSION
// setvbuf() test driver is PDCLib-specific
TEST(stdio, setvbuf)
{
    char buffer[BUFFERSIZE];

    /* full buffered, user-supplied buffer */
    {
        FILE* fh = tmpfile();
        ASSERT_NE(NULL, fh);
        EXPECT_EQ(0, setvbuf(fh, buffer, _IOFBF, BUFFERSIZE));
        EXPECT_EQ(buffer, fh->buffer);
        EXPECT_EQ(BUFFERSIZE, fh->bufsize);
                EXPECT_EQ(_IOFBF, ( fh->status & ( _IOFBF | _IONBF | _IOLBF ));
		EXPECT_EQ(0, fclose( fh ));
    }
    /* line buffered, lib-supplied buffer */
    {
        FILE* fh = tmpfile();
        ASSERT_NE(NULL, fh);

        EXPECT_EQ(0, setvbuf(fh, NULL, _IOLBF, BUFFERSIZE));
        EXPECT_NE(NULL, fh->buffer);
        EXPECT_EQ(BUFFERSIZE, fh->bufsize);
        EXPECT_EQ(_IOLBF, (fh->status & (_IOFBF | _IONBF | _IOLBF)));
        EXPECT_EQ(0, fclose(fh));
    }
    /* not buffered, user-supplied buffer */
    {
        FILE* fh = tmpfile();
        ASSERT_NE(NULL, fh);
        EXPECT_EQ(0, setvbuf(fh, buffer, _IONBF, BUFFERSIZE));
                EXPECT_EQ(_IONBF, ( fh->status & ( _IOFBF | _IONBF | _IOLBF ) );
		EXPECT_EQ(0, fclose(fh));
    }
}
#endif

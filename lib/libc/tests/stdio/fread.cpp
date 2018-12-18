#include <gtest/gtest.h>
#include <stdio.h>

TEST(stdio, fread)
{
    char const* message = "Testing fwrite()...\n";
    char buffer[21];
    buffer[20] = 'x';

    FILE* fh = tmpfile();
    ASSERT_NE(nullptr, fh);
    /* fwrite() / readback */
    EXPECT_EQ(20, fwrite(message, 1, 20, fh));
    rewind(fh);
    EXPECT_EQ(20, fread(buffer, 1, 20, fh));
    EXPECT_EQ(0, memcmp(buffer, message, 20));
    EXPECT_EQ('x', buffer[20]);
    /* same, different nmemb / size settings */
    rewind(fh);
    EXPECT_EQ(buffer, memset(buffer, '\0', 20));
    EXPECT_EQ(4, fwrite(message, 5, 4, fh));
    rewind(fh);
    EXPECT_EQ(4, fread(buffer, 5, 4, fh));
    EXPECT_EQ(0, memcmp(buffer, message, 20));
    EXPECT_EQ('x', buffer[20]);
    /* same... */
    rewind(fh);
    EXPECT_EQ(buffer, memset(buffer, '\0', 20));
    EXPECT_EQ(1, fwrite(message, 20, 1, fh));
    rewind(fh);
    EXPECT_EQ(1, fread(buffer, 20, 1, fh));
    EXPECT_EQ(0, memcmp(buffer, message, 20));
    EXPECT_EQ('x', buffer[20]);
    /* Done. */
    EXPECT_EQ(0, fclose(fh));
}

#include <gtest/gtest.h>
#include <stdio.h>

TEST(stdio, fputs)
{
    char const* const message = "SUCCESS testing fputs()";
    FILE* fh = tmpfile();
    ASSERT_NE(nullptr, fh);

    EXPECT_GE(fputs(message, fh), 0);
    rewind(fh);
    for (size_t i = 0; i < 23; ++i) {
        EXPECT_EQ(message[i], fgetc(fh));
    }
    EXPECT_EQ(0, fclose(fh));
}

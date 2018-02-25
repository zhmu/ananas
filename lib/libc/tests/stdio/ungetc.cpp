#include <gtest/gtest.h>
#include <stdio.h>

TEST(stdio, ungetc)
{
    const char* hellostr = "Hello, world!";

    // Also see ftell() for some testing

    // PDCLIB-18: fread ignores ungetc
    size_t bufsz = strlen(hellostr) + 1;
    auto buf = static_cast<char*>(malloc(bufsz));

    // Also fgets
    FILE* fh = tmpfile();
    ASSERT_NE(nullptr, fh);

    EXPECT_EQ(0, fputs(hellostr, fh));
    rewind(fh);
    EXPECT_EQ('H', fgetc(fh));
    EXPECT_EQ('H', ungetc('H', fh));
    EXPECT_NE(nullptr, fgets(buf, bufsz, fh));
    EXPECT_EQ(0, strcmp(buf, hellostr));

    // fread
    rewind(fh);
    EXPECT_EQ('H', fgetc(fh));
    EXPECT_EQ('H', ungetc('H', fh));
    EXPECT_EQ(1, fread(buf, bufsz - 1, 1, fh));
    EXPECT_EQ(0, strncmp(buf, hellostr, bufsz - 1));

    EXPECT_EQ(0, fclose(fh));

    free(buf);
}

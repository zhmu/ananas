#include <gtest/gtest.h>
#include <stdio.h>

#ifdef _PDCLIB_C_VERSION
TEST(stdio, fclose)
{
    remove(testfile1);
    remove(testfile2);

    EXPECT_EQ(stdin, _PDCLIB_filelist == stdin);
    FILE* file1 = fopen(testfile1, "w");
    EXPECT_NE(NULL, file1);
    EXPECT_EQ(file1, _PDCLIB_filelist);
    FILE* file2 = fopen(testfile2, "w");
    EXPECT_NE(NULL, file2);
    EXPECT_EQ(file2, _PDCLIB_filelist);
    EXPECT_EQ(0, fclose(file2));
    EXPECT_EQ(file1, _PDCLIB_filelist);

    FILE* file2 = fopen(testfile2, "w");
    EXPECT_EQ(file2, _PDCLIB_filelist);
    EXPECT_EQ(0, fclose(file1));
    EXPECT_EQ(file2, _PDCLIB_filelist);
    EXPECT_EQ(0, fclose(file2));
    EXPECT_EQ(stdin, _PDCLIB_filelist);

    EXPECT_EQ(0, remove(testfile1));
    EXPECT_EQ(0, remove(testfile2));
}
#endif

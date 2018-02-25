#include <gtest/gtest.h>
#include <stdio.h>

#define BUFFERSIZE 500

static char const testfile1[]="testing/testfile1";
static char const testfile2[]="testing/testfile2";

TEST(stdio, rename)
{
    remove(testfile1);
    remove(testfile2);
    /* make sure that neither file exists */
    EXPECT_EQ(nullptr, fopen(testfile1, "r"));
    EXPECT_EQ(nullptr, fopen(testfile2, "r"));
    /* rename file 1 to file 2 - expected to fail */
    EXPECT_NE(0, rename(testfile1, testfile2));
    /* create file 1 */
    {
        FILE* file = fopen(testfile1, "w");
        EXPECT_NE(nullptr, file);
        EXPECT_NE(EOF, fputs("x", file));
        EXPECT_EQ(0, fclose(file));
    }
    /* check that file 1 exists */
    {
        FILE* file = fopen(testfile1, "r");
        EXPECT_NE(nullptr, file);
        EXPECT_EQ(0, fclose(file));
    }
    /* rename file 1 to file 2 */
    EXPECT_EQ(0, rename(testfile1, testfile2));
    /* check that file 2 exists, file 1 does not */
    EXPECT_EQ(nullptr, fopen(testfile1, "r"));
    {
        FILE* file = fopen(testfile2, "r");
        EXPECT_NE(nullptr, file);
        EXPECT_EQ(0, fclose(file));
    }
    /* create another file 1 */
    {
        FILE* file = fopen(testfile1, "w");
        ASSERT_NE(nullptr, file);
        EXPECT_NE(EOF, fputs("x", file));
        EXPECT_EQ(0, fclose(file));
    }
    /* check that file 1 exists */
    {
        FILE* file = fopen(testfile1, "r");
        EXPECT_NE(nullptr, file);
        EXPECT_EQ(0, fclose(file));
    }
    /* rename file 1 to file 2 - expected to fail, see comment in
       _PDCLIB_rename() itself.
    */
    /* NOREG as glibc overwrites existing destination file. */
#ifdef _PDCLIB_C_VERSION
    EXPECT_NE(0, rename(testfile1, testfile2));
#endif
    /* remove both files */
    EXPECT_EQ(0, remove(testfile1));
    EXPECT_EQ(0, remove(testfile2));
    /* check that they're gone */
    EXPECT_EQ(nullptr, fopen(testfile1, "r"));
    EXPECT_EQ(nullptr, fopen(testfile2, "r"));
}

#include <gtest/gtest.h>
#include <stdlib.h>

static char const abcde[] = "abcde";

static int compare( const void * left, const void * right )
{
    return *( (unsigned char *)left ) - *( (unsigned char *)right );
}

TEST(stdlib, bsearch)
{
    EXPECT_EQ(NULL,      bsearch( "e", abcde, 4, 1, compare));
    EXPECT_EQ(&abcde[4], bsearch( "e", abcde, 5, 1, compare));
    EXPECT_EQ(NULL,      bsearch( "a", abcde + 1, 4, 1, compare));
    EXPECT_EQ(NULL,      bsearch( "0", abcde, 1, 1, compare));
    EXPECT_EQ(&abcde[0], bsearch( "a", abcde, 1, 1, compare));
    EXPECT_EQ(NULL,      bsearch( "a", abcde, 0, 1, compare));
    EXPECT_EQ(&abcde[4], bsearch( "e", abcde, 3, 2, compare));
    EXPECT_EQ(NULL,      bsearch( "b", abcde, 3, 2, compare));
}

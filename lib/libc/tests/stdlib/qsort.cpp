#include <gtest/gtest.h>
#include <stdlib.h>

static int compare( const void * left, const void * right )
{
    return *( (unsigned char *)left ) - *( (unsigned char *)right );
}

TEST(stdlib, qsort)
{
    char presort[] = { "shreicnyjqpvozxmbt" };
    char sorted1[] = { "bcehijmnopqrstvxyz" };
    char sorted2[] = { "bticjqnyozpvreshxm" };
    char s[19];
    strcpy(s, presort);

    qsort(s, 18, 1, compare);
    EXPECT_EQ(0, strcmp(s, sorted1));
    strcpy( s, presort );
    qsort( s, 9, 2, compare );
    EXPECT_EQ(0, strcmp(s, sorted2));
    strcpy( s, presort );
    qsort( s, 1, 1, compare );
    EXPECT_EQ(0, strcmp(s, presort));
#if defined(REGTEST) && (__BSD_VISIBLE || __APPLE__)
    #error "qsort: Skipping test #4 for BSD as it goes into endless loop here." // checkme
#else
    qsort( s, 100, 0, compare );
    EXPECT_EQ(0, strcmp(s, presort));
#endif
}

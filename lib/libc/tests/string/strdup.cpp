#include <gtest/gtest.h>
#include <string.h>

TEST(string, strdup)
{
    const char* teststr = "Hello, world";
    const char* teststr2 = "An alternative test string with non-7-bit characters \xFE\x8C\n";

    char* testres = strdup(teststr);
    ASSERT_NE(nullptr, testres);
    char* testres2 = strdup(teststr2);
    ASSERT_NE(nullptr, testres2);
    EXPECT_EQ(0, strcmp(testres, teststr));
    EXPECT_EQ(0, strcmp(testres2, teststr2));
    free(testres);
    free(testres2);
}

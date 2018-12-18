#include <gtest/gtest.h>
#include <string.h>

TEST(string, strndup)
{
    /* Missing on Windows. Maybe use conditionals? */
    const char* teststr = "Hello, world";
    const char* teststr2 = "\xFE\x8C\n";

    {
        char* testres = strndup(teststr, 5);
        ASSERT_NE(nullptr, testres);
        char* testres2 = strndup(teststr2, 1);
        ASSERT_NE(nullptr, testres2);
        EXPECT_NE(0, strcmp(testres, teststr));
        EXPECT_EQ(0, strncmp(testres, teststr, 5));
        EXPECT_NE(0, strcmp(testres2, teststr2));
        EXPECT_EQ(0, strncmp(testres2, teststr2, 1));
        free(testres);
        free(testres2);
    }
    {
        char* testres = strndup(teststr, 20);
        ASSERT_NE(nullptr, testres);
        char* testres2 = strndup(teststr2, 5);
        ASSERT_NE(nullptr, testres2);
        EXPECT_EQ(0, strcmp(testres, teststr));
        EXPECT_EQ(0, strcmp(testres2, teststr2));
        free(testres);
        free(testres2);
    }
}

#include <gtest/gtest.h>
#include <string.h>

TEST(string, strerror) { EXPECT_NE(strerror(ERANGE), strerror(EDOM)); }

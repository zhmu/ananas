// SUMMARY:See if opening nonexistent files fails
#include "framework.h"
#include <fcntl.h>

TEST_BODY_BEGIN
{
    int fd = open("nonexistent", O_RDONLY);
    EXPECT_EQ(-1, fd);
}
TEST_BODY_END

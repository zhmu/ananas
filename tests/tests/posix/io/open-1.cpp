// SUMMARY:See if opening nonexistent files fails
#include "framework.h"
#include <fcntl.h>

TEST_BODY_BEGIN
{
	// We try multiple times to account for caching and such
	for (int n = 0; n < 10; n++) {
		int fd = open("nonexistent", O_RDONLY);
		EXPECT_EQ(-1, fd);
	}
}
TEST_BODY_END

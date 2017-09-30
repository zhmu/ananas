// SUMMARY:Reading from NULL crashes

#include "framework.h"

TEST_BODY_BEGIN
{
	volatile int* p = nullptr;
	volatile int a;
	ASSERT_DEATH(a = *p);
	// We shouldn't reach this, but we must not let 'a' get optimized away
	ASSERT_EQ(a, 0xdead);
}
TEST_BODY_END

#include "framework.h"
#include <stdio.h>

namespace TestFramework
{

void
TestFailure(const char* file, int line)
{
	fprintf(stderr, "FAILURE: %s:%d\n", file, line);
}

void
TestBegin(const char* file)
{
	fprintf(stderr, "START: %s\n", file);
}

void
TestEnd(const char* file)
{
	fprintf(stderr, "END: %s\n", file);
}

void
TestAbort(const char* file, int line)
{
	fprintf(stderr, "ABORT: %s:%d\n", file, line);
}

void
TestExpectDeath(const char* file, int line)
{
	fprintf(stderr, "EXPECT-DEATH: %s:%d\n", file, line);
}

} // namespace TestFramework

int
main(int argc, char* argv[])
{
	// 0 means test was successful
	return TestFramework::TestBody() == TestFramework::TR_Success ? 0 : 1;
}

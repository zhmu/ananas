#include "test-framework.h"

void queue_test();
void dqueue_test();

int
main()
{
	framework_init();
	queue_test();
	dqueue_test();
	framework_done();
	return 0;
}

#include "test-framework.h"

void queue_test();
void dqueue_test();
void cbuffer_test();

int
main()
{
	framework_init();
	queue_test();
	dqueue_test();
	cbuffer_test();
	framework_done();
	return 0;
}

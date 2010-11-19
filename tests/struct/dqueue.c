#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../include/ananas/dqueue.h"

struct test_item {
	int value;
	DQUEUE_FIELDS(struct test_item);
};

DQUEUE_DEFINE(test_queue, struct test_item);

int
main()
{
	struct test_queue tq;
	DQUEUE_INIT(&tq);

	/* Initial queues must be empty */
	assert(DQUEUE_EMPTY(&tq));

	struct test_item ti[5];
	ti[0].value = 0;
	ti[1].value = 1;
	ti[2].value = 2;
	ti[3].value = 3;
	ti[4].value = 4;

	/* Add an item; queue must not be empty */
	DQUEUE_ADD_TAIL(&tq, &ti[0])
	assert(!DQUEUE_EMPTY(&tq));
	DQUEUE_ADD_TAIL(&tq, &ti[1]);
	assert(!DQUEUE_EMPTY(&tq));
	DQUEUE_ADD_TAIL(&tq, &ti[2]);
	assert(!DQUEUE_EMPTY(&tq));

	/* Check whether the first item is on top */
	assert(DQUEUE_HEAD(&tq) == &ti[0]);

	/* Removing the first item must not empty the queue */
	DQUEUE_POP_HEAD(&tq);
	assert(!DQUEUE_EMPTY(&tq));

	/* Removing the second item must not empty the queue */
	assert(DQUEUE_HEAD(&tq) == &ti[1]);
	assert(DQUEUE_PREV(DQUEUE_HEAD(&tq)) == NULL);
	DQUEUE_POP_HEAD(&tq);
	assert(!DQUEUE_EMPTY(&tq));

	/* Removing the final item must empty the queue */
	assert(DQUEUE_HEAD(&tq) == &ti[2]);
	DQUEUE_POP_HEAD(&tq);
	assert(DQUEUE_EMPTY(&tq));

	/* Fill the queue again */
	DQUEUE_ADD_TAIL(&tq, &ti[0]);
	DQUEUE_ADD_TAIL(&tq, &ti[1]);
	DQUEUE_ADD_TAIL(&tq, &ti[2]);

	struct test_item* it = DQUEUE_HEAD(&tq);
	assert(it == &ti[0]);
	it = DQUEUE_NEXT(it); assert(it == &ti[1]);
	it = DQUEUE_NEXT(it); assert(it == &ti[2]);
	it = DQUEUE_NEXT(it); assert(it == NULL);

	/* Test the foreach macro */
	int i = 0;
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		assert(iter == &ti[i]);
		i++;
	}
	
	/* Test the foreach in reverse macro */
	i = 2;
	DQUEUE_FOREACH_REVERSE(&tq, iter, struct test_item) {
		assert(iter == &ti[i]);
		i--;
	}

	/* Add some extra items */
	DQUEUE_ADD_TAIL(&tq, &ti[3]);
	DQUEUE_ADD_TAIL(&tq, &ti[4]);

	/* Implicitely remove the head */
	DQUEUE_REMOVE(&tq, &ti[0]);
	assert(DQUEUE_HEAD(&tq) == &ti[1]);
	assert(DQUEUE_PREV(DQUEUE_HEAD(&tq)) == NULL);

	/* Remove the second entry; queue is now 1, 3, 4 */
	DQUEUE_REMOVE(&tq, &ti[2]);

	i = 0;
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		switch(i) {
			case 0: assert(iter == &ti[1]); break;
			case 1: assert(iter == &ti[3]); break;
			case 2: assert(iter == &ti[4]); break;
			default: assert(0);
		}
		i++;
	}
	assert(i == 3);

	/* Remove the final item */
	DQUEUE_POP_TAIL(&tq);
	it = DQUEUE_HEAD(&tq); assert(it == &ti[1]); assert(DQUEUE_PREV(it) == NULL);
	it = DQUEUE_NEXT(it);  assert(it == &ti[3]); assert(DQUEUE_PREV(it) == &ti[1]);
	it = DQUEUE_NEXT(it);  assert(it == NULL);

	/* Implicitely remove the final item */
	DQUEUE_REMOVE(&tq, &ti[3]);
	it = DQUEUE_HEAD(&tq); assert(it == &ti[1]); assert(DQUEUE_PREV(it) == NULL);
	it = DQUEUE_NEXT(it);  assert(it == NULL);

	return 0;
}

/* vim:set ts=2 sw=2: */

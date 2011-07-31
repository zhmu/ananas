#include <stdlib.h>
#include <stdio.h>
#include <ananas/queue.h>
#include "test-framework.h"

struct test_item {
	int value;
	QUEUE_FIELDS(struct test_item);
};

QUEUE_DEFINE(test_queue, struct test_item);

void
queue_test()
{
	struct test_queue tq;
	QUEUE_INIT(&tq);

	/* Initial queues must be empty */
	EXPECT(QUEUE_EMPTY(&tq));

	struct test_item ti[3];
	ti[0].value = 0;
	ti[1].value = 1;
	ti[2].value = 2;

	/* Add an item; queue must not be empty */
	QUEUE_ADD_TAIL(&tq, &ti[0]);
	EXPECT(!QUEUE_EMPTY(&tq));
	QUEUE_ADD_TAIL(&tq, &ti[1]);
	EXPECT(!QUEUE_EMPTY(&tq));
	QUEUE_ADD_TAIL(&tq, &ti[2]);
	EXPECT(!QUEUE_EMPTY(&tq));

	/* Check whether the first item is on top */
	EXPECT(QUEUE_HEAD(&tq) == &ti[0]);

	/* Removing the first item must not empty the queue */
	QUEUE_POP_HEAD(&tq);
	EXPECT(!QUEUE_EMPTY(&tq));

	/* Removing the second item must not empty the queue */
	EXPECT(QUEUE_HEAD(&tq) == &ti[1]);
	QUEUE_POP_HEAD(&tq);
	EXPECT(!QUEUE_EMPTY(&tq));

	/* Removing the final item must empty the queue */
	EXPECT(QUEUE_HEAD(&tq) == &ti[2]);
	QUEUE_POP_HEAD(&tq);
	EXPECT(QUEUE_EMPTY(&tq));

	/* Fill the queue again */
	QUEUE_ADD_TAIL(&tq, &ti[0]);
	QUEUE_ADD_TAIL(&tq, &ti[1]);
	QUEUE_ADD_TAIL(&tq, &ti[2]);

	struct test_item* it = QUEUE_HEAD(&tq); EXPECT(it == &ti[0]);
	it = QUEUE_NEXT(it); EXPECT(it == &ti[1]);
	it = QUEUE_NEXT(it); EXPECT(it == &ti[2]);
	it = QUEUE_NEXT(it); EXPECT(it == NULL);

	int i = 0;
	QUEUE_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i++;
	}
}

/* vim:set ts=2 sw=2: */

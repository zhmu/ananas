#include <stdlib.h>
#include <stdio.h>
#include <ananas/dqueue.h>
#include "test-framework.h"

struct test_item {
	int value;
	DQUEUE_FIELDS(struct test_item);
};

DQUEUE_DEFINE(test_queue, struct test_item);

void
dqueue_test()
{
	struct test_queue tq;
	DQUEUE_INIT(&tq);

	/* Initial queues must be empty */
	EXPECT(DQUEUE_EMPTY(&tq));

	struct test_item ti[5];
	ti[0].value = 0;
	ti[1].value = 1;
	ti[2].value = 2;
	ti[3].value = 3;
	ti[4].value = 4;

	/* Add an item; queue must not be empty */
	DQUEUE_ADD_TAIL(&tq, &ti[0]);
	EXPECT(!DQUEUE_EMPTY(&tq));
	DQUEUE_ADD_TAIL(&tq, &ti[1]);
	EXPECT(!DQUEUE_EMPTY(&tq));
	DQUEUE_ADD_TAIL(&tq, &ti[2]);
	EXPECT(!DQUEUE_EMPTY(&tq));

	/* Check whether the first item is on top */
	EXPECT(DQUEUE_HEAD(&tq) == &ti[0]);

	/* Removing the first item must not empty the queue */
	DQUEUE_POP_HEAD(&tq);
	EXPECT(!DQUEUE_EMPTY(&tq));

	/* Removing the second item must not empty the queue */
	EXPECT(DQUEUE_HEAD(&tq) == &ti[1]);
	EXPECT(DQUEUE_PREV(DQUEUE_HEAD(&tq)) == NULL);
	DQUEUE_POP_HEAD(&tq);
	EXPECT(!DQUEUE_EMPTY(&tq));

	/* Removing the final item must empty the queue */
	EXPECT(DQUEUE_HEAD(&tq) == &ti[2]);
	DQUEUE_POP_HEAD(&tq);
	EXPECT(DQUEUE_EMPTY(&tq));

	/* Fill the queue again */
	DQUEUE_ADD_TAIL(&tq, &ti[0]);
	DQUEUE_ADD_TAIL(&tq, &ti[1]);
	DQUEUE_ADD_TAIL(&tq, &ti[2]);

	struct test_item* it = DQUEUE_HEAD(&tq);
	EXPECT(it == &ti[0]);
	it = DQUEUE_NEXT(it); EXPECT(it == &ti[1]);
	it = DQUEUE_NEXT(it); EXPECT(it == &ti[2]);
	it = DQUEUE_NEXT(it); EXPECT(it == NULL);

	/* Test the foreach macro */
	int i = 0;
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i++;
	}
	
	/* Test the foreach in reverse macro */
	i = 2;
	DQUEUE_FOREACH_REVERSE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i--;
	}

	/* Add some extra items */
	DQUEUE_ADD_TAIL(&tq, &ti[3]);
	DQUEUE_ADD_TAIL(&tq, &ti[4]);

	/* Implicitely remove the head */
	DQUEUE_REMOVE(&tq, &ti[0]);
	EXPECT(DQUEUE_HEAD(&tq) == &ti[1]);
	EXPECT(DQUEUE_PREV(DQUEUE_HEAD(&tq)) == NULL);

	/* Remove the second entry; queue is now 1, 3, 4 */
	DQUEUE_REMOVE(&tq, &ti[2]);

	i = 0;
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		switch(i) {
			case 0: EXPECT(iter == &ti[1]); break;
			case 1: EXPECT(iter == &ti[3]); break;
			case 2: EXPECT(iter == &ti[4]); break;
			default: EXPECT(0);
		}
		i++;
	}
	EXPECT(i == 3);

	/* Remove the final item */
	DQUEUE_POP_TAIL(&tq);
	it = DQUEUE_HEAD(&tq); EXPECT(it == &ti[1]); EXPECT(DQUEUE_PREV(it) == NULL);
	it = DQUEUE_NEXT(it);  EXPECT(it == &ti[3]); EXPECT(DQUEUE_PREV(it) == &ti[1]);
	it = DQUEUE_NEXT(it);  EXPECT(it == NULL);

	/* Implicitely remove the final item */
	DQUEUE_REMOVE(&tq, &ti[3]);
	it = DQUEUE_HEAD(&tq); EXPECT(it == &ti[1]); EXPECT(DQUEUE_PREV(it) == NULL);
	it = DQUEUE_NEXT(it);  EXPECT(it == NULL);

	/* Start over and pollute the list */
	DQUEUE_INIT(&tq);
	DQUEUE_ADD_TAIL(&tq, &ti[0]);
	DQUEUE_ADD_TAIL(&tq, &ti[1]);
	DQUEUE_ADD_TAIL(&tq, &ti[2]);
	DQUEUE_ADD_TAIL(&tq, &ti[3]);
	DQUEUE_ADD_TAIL(&tq, &ti[4]);

	/* Check whether the safe version works out */
	i = 0;
	DQUEUE_FOREACH_SAFE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		if(i % 2 == 1)
			DQUEUE_REMOVE(&tq, iter);
		i++;
	}
	EXPECT(i == 5);

	/* See if the correct things were removed */
	i = 0;
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		switch(i) {
			case 0: EXPECT(iter == &ti[0]); break;
			case 1: EXPECT(iter == &ti[2]); break;
			case 2: EXPECT(iter == &ti[4]); break;
			default: EXPECT(0);
		}
		i++;
	}
	EXPECT(i == 3);

	/* Try the reverse version */
	i = 0;
	DQUEUE_FOREACH_REVERSE_SAFE(&tq, iter, struct test_item) {
		if (i == 1)
			DQUEUE_REMOVE(&tq, iter);
		i++;
	}
	EXPECT(i == 3);

	/* And see if we end up with what we expect */
	i = 0;
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		switch(i) {
			case 0: EXPECT(iter == &ti[0]); break;
			case 1: EXPECT(iter == &ti[4]); break;
			default: EXPECT(0);
		}
		i++;
	}
	EXPECT(i == 2);

	/* Check whether adding elements to the head works as intended */
	DQUEUE_INIT(&tq);
	DQUEUE_ADD_HEAD(&tq, &ti[0]);
	DQUEUE_ADD_HEAD(&tq, &ti[1]);
	DQUEUE_ADD_HEAD(&tq, &ti[2]);
	DQUEUE_ADD_HEAD(&tq, &ti[3]);
	DQUEUE_ADD_HEAD(&tq, &ti[4]);
	i = 5;
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i - 1]);
		i--;
	}
	EXPECT(i == 0);
	DQUEUE_FOREACH_REVERSE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i++;
	}
	EXPECT(i == 5);

	/* OK; now see whether inserting at the beginning works */
	DQUEUE_INIT(&tq);
	DQUEUE_ADD_HEAD(&tq, &ti[3]);
	DQUEUE_INSERT_BEFORE(&tq, &ti[3], &ti[1]);
	i = 0;
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[(2 * i) + 1]);
		i++;
	}
	EXPECT(i == 2);
	DQUEUE_FOREACH_REVERSE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[(i - 1) * 2 + 1]);
		i--;
	}
	EXPECT(i == 0);
	/* Insert at the center */
	DQUEUE_INSERT_BEFORE(&tq, &ti[3], &ti[2]);
	DQUEUE_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i + 1]);
		i++;
	}
	EXPECT(i == 3);
	DQUEUE_FOREACH_REVERSE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i--;
	}
	EXPECT(i == 0);
}

/* vim:set ts=2 sw=2: */

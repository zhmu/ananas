#include <stdlib.h>
#include <stdio.h>
#include <ananas/list.h>
#include "test-framework.h"

struct test_item {
	int value;
	LIST_FIELDS(struct test_item);
};

LIST_DEFINE(test_queue, struct test_item);

void
dqueue_test()
{
	struct test_queue tq;
	LIST_INIT(&tq);

	/* Initial queues must be empty */
	EXPECT(LIST_EMPTY(&tq));

	struct test_item ti[5];
	ti[0].value = 0;
	ti[1].value = 1;
	ti[2].value = 2;
	ti[3].value = 3;
	ti[4].value = 4;

	/* Add an item; queue must not be empty */
	LIST_APPEND(&tq, &ti[0]);
	EXPECT(!LIST_EMPTY(&tq));
	LIST_APPEND(&tq, &ti[1]);
	EXPECT(!LIST_EMPTY(&tq));
	LIST_APPEND(&tq, &ti[2]);
	EXPECT(!LIST_EMPTY(&tq));

	/* Check whether the first item is on top */
	EXPECT(LIST_HEAD(&tq) == &ti[0]);

	/* Removing the first item must not empty the queue */
	LIST_POP_HEAD(&tq);
	EXPECT(!LIST_EMPTY(&tq));

	/* Removing the second item must not empty the queue */
	EXPECT(LIST_HEAD(&tq) == &ti[1]);
	EXPECT(LIST_PREV(LIST_HEAD(&tq)) == NULL);
	LIST_POP_HEAD(&tq);
	EXPECT(!LIST_EMPTY(&tq));

	/* Removing the final item must empty the queue */
	EXPECT(LIST_HEAD(&tq) == &ti[2]);
	LIST_POP_HEAD(&tq);
	EXPECT(LIST_EMPTY(&tq));

	/* Fill the queue again */
	LIST_APPEND(&tq, &ti[0]);
	LIST_APPEND(&tq, &ti[1]);
	LIST_APPEND(&tq, &ti[2]);

	struct test_item* it = LIST_HEAD(&tq);
	EXPECT(it == &ti[0]);
	it = LIST_NEXT(it); EXPECT(it == &ti[1]);
	it = LIST_NEXT(it); EXPECT(it == &ti[2]);
	it = LIST_NEXT(it); EXPECT(it == NULL);

	/* Test the foreach macro */
	int i = 0;
	LIST_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i++;
	}
	
	/* Test the foreach in reverse macro */
	i = 2;
	LIST_FOREACH_REVERSE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i--;
	}

	/* Add some extra items */
	LIST_APPEND(&tq, &ti[3]);
	LIST_APPEND(&tq, &ti[4]);

	/* Implicitely remove the head */
	LIST_REMOVE(&tq, &ti[0]);
	EXPECT(LIST_HEAD(&tq) == &ti[1]);
	EXPECT(LIST_PREV(LIST_HEAD(&tq)) == NULL);

	/* Remove the second entry; queue is now 1, 3, 4 */
	LIST_REMOVE(&tq, &ti[2]);

	i = 0;
	LIST_FOREACH(&tq, iter, struct test_item) {
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
	LIST_POP_TAIL(&tq);
	it = LIST_HEAD(&tq); EXPECT(it == &ti[1]); EXPECT(LIST_PREV(it) == NULL);
	it = LIST_NEXT(it);  EXPECT(it == &ti[3]); EXPECT(LIST_PREV(it) == &ti[1]);
	it = LIST_NEXT(it);  EXPECT(it == NULL);

	/* Implicitely remove the final item */
	LIST_REMOVE(&tq, &ti[3]);
	it = LIST_HEAD(&tq); EXPECT(it == &ti[1]); EXPECT(LIST_PREV(it) == NULL);
	it = LIST_NEXT(it);  EXPECT(it == NULL);

	/* Start over and pollute the list */
	LIST_INIT(&tq);
	LIST_APPEND(&tq, &ti[0]);
	LIST_APPEND(&tq, &ti[1]);
	LIST_APPEND(&tq, &ti[2]);
	LIST_APPEND(&tq, &ti[3]);
	LIST_APPEND(&tq, &ti[4]);

	/* Check whether the safe version works out */
	i = 0;
	LIST_FOREACH_SAFE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		if(i % 2 == 1)
			LIST_REMOVE(&tq, iter);
		i++;
	}
	EXPECT(i == 5);

	/* See if the correct things were removed */
	i = 0;
	LIST_FOREACH(&tq, iter, struct test_item) {
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
	LIST_FOREACH_REVERSE_SAFE(&tq, iter, struct test_item) {
		if (i == 1)
			LIST_REMOVE(&tq, iter);
		i++;
	}
	EXPECT(i == 3);

	/* And see if we end up with what we expect */
	i = 0;
	LIST_FOREACH(&tq, iter, struct test_item) {
		switch(i) {
			case 0: EXPECT(iter == &ti[0]); break;
			case 1: EXPECT(iter == &ti[4]); break;
			default: EXPECT(0);
		}
		i++;
	}
	EXPECT(i == 2);

	/* Check whether adding elements to the head works as intended */
	LIST_INIT(&tq);
	LIST_PREPEND(&tq, &ti[0]);
	LIST_PREPEND(&tq, &ti[1]);
	LIST_PREPEND(&tq, &ti[2]);
	LIST_PREPEND(&tq, &ti[3]);
	LIST_PREPEND(&tq, &ti[4]);
	i = 5;
	LIST_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i - 1]);
		i--;
	}
	EXPECT(i == 0);
	LIST_FOREACH_REVERSE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i++;
	}
	EXPECT(i == 5);

	/* OK; now see whether inserting at the beginning works */
	LIST_INIT(&tq);
	LIST_PREPEND(&tq, &ti[3]);
	LIST_INSERT_BEFORE(&tq, &ti[3], &ti[1]);
	i = 0;
	LIST_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[(2 * i) + 1]);
		i++;
	}
	EXPECT(i == 2);
	LIST_FOREACH_REVERSE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[(i - 1) * 2 + 1]);
		i--;
	}
	EXPECT(i == 0);
	/* Insert at the center */
	LIST_INSERT_BEFORE(&tq, &ti[3], &ti[2]);
	LIST_FOREACH(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i + 1]);
		i++;
	}
	EXPECT(i == 3);
	LIST_FOREACH_REVERSE(&tq, iter, struct test_item) {
		EXPECT(iter == &ti[i]);
		i--;
	}
	EXPECT(i == 0);
}

/* vim:set ts=2 sw=2: */

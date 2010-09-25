#ifndef __ANANAS_QUEUE_H__
#define __ANANAS_QUEUE_H__

/*
 * A queue is a singly-linked structure; obtaining/removing the head takes
 * O(1), adding a new item takes O(1). Iterating through the entire queue takes
 * O(n) (this also holds for locating a single item).
 *
 * Each queue has a 'head' and 'tail' element. It should be used like this:
 *
 * struct test_item {
 *   ...
 *   QUEUE_FIELDS;
 * };
 * QUEUE_DEFINE(test_queue);
 *
 * void main() {
 *   struct test_queue t;
 *   QUEUE_INIT(&t);
 *
 *   struct test_item* ti;

 *   QUEUE_ADD_TAIL(&t, ti, struct test_queue);
 *   QUEUE_ADD_TAIL(&t, ti, struct test_queue);
 *   while (!QUEUE_EMPTY(&t)) {
 *     struct test_item* headitem = QUEUE_HEAD(&t, struct test_queue);
 *     QUEUE_POP_HEAD(&t, struct test_queue);
 *     ...
 *   }
 * }
 */

#define QUEUE_FIELDS						\
	void* qi_next;

#define QUEUE_DEFINE(name)					\
	struct name {						\
		void* head;					\
		void* tail;					\
	};

#define QUEUE_INIT(q) 						\
	(q)->head = NULL;					\
	(q)->tail = NULL;					\

#define QUEUE_ADD_TAIL(q, item, TYPE)				\
	(item)->qi_next = NULL;					\
	if ((q)->head == NULL) {				\
		(q)->head = (item);				\
	} else {						\
		((TYPE*)(q)->tail)->qi_next = (item);		\
	}							\
	(q)->tail = (item);

#define QUEUE_HEAD(q, TYPE) 					\
	(TYPE*)((q)->head)

#define QUEUE_POP_HEAD(q, TYPE)					\
	(q)->head = ((TYPE*)(q)->head)->qi_next;

#define QUEUE_EMPTY(q)						\
	((q)->head == NULL)

#endif /* __ANANAS_QUEUE_H__ */

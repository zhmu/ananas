#ifndef __ANANAS_QUEUE_H__
#define __ANANAS_QUEUE_H__

/*
 * A queue is a singly-linked structure; obtaining/removing the head takes
 * O(1), adding a new item takes O(1). Iterating through the entire queue takes
 * O(n) (this also holds for locating a single item).
 *
 * Usage instructions can be found in the 'queue' structure test.
 */

#define QUEUE_FIELDS(TYPE)					\
	TYPE* qi_next;

#define QUEUE_DEFINE(name, TYPE)				\
	struct name {						\
		TYPE* head;					\
		TYPE* tail;					\
	};

#define QUEUE_INIT(q) 						\
do {								\
	(q)->head = NULL;					\
	(q)->tail = NULL;					\
} while(0)

#define QUEUE_ADD_TAIL(q, item)					\
do {								\
	(item)->qi_next = NULL;					\
	if ((q)->head == NULL) {				\
		(q)->head = (item);				\
	} else {						\
		(q)->tail->qi_next = (item);			\
	}							\
	(q)->tail = (item);					\
} while(0)

#define QUEUE_HEAD(q) 						\
	((q)->head)

#define QUEUE_POP_HEAD(q)					\
	(q)->head = (q)->head->qi_next;

#define QUEUE_EMPTY(q)						\
	((q)->head == NULL)

#define QUEUE_NEXT(it)						\
	(it)->qi_next

#define QUEUE_FOREACH(q, it, TYPE)				\
	for (TYPE* (it) = QUEUE_HEAD(q);			\
	     (it) != NULL;					\
	     (it) = QUEUE_NEXT(it))

#endif /* __ANANAS_QUEUE_H__ */

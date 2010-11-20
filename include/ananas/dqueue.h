#ifndef __ANANAS_DQUEUE_H__
#define __ANANAS_DQUEUE_H__

/*
 * A dqueue is a doubly-linked structure; obtaining/removing the head takes
 * O(1), adding a new item takes O(1) and removing any item takes O(1).
 * Iterating through the entire queue takes O(n) (this also holds for locating
 * a single item).
 *
 * Each queue has a 'head' and 'tail' element, and every item has a previous
 * and next pointer.
 *
 * Usage instructions can be found in the 'dqueue' structure test.
 */

#define DQUEUE_FIELDS(TYPE)					\
	TYPE* qi_prev;						\
	TYPE* qi_next;

#define DQUEUE_DEFINE(name, TYPE)				\
	struct name {						\
		TYPE* dq_head;					\
		TYPE* dq_tail;					\
	};

#define DQUEUE_INIT(q) 						\
do {								\
	(q)->dq_head = NULL;					\
	(q)->dq_tail = NULL;					\
} while(0)

#define DQUEUE_ADD_TAIL(q, item)				\
do {								\
	(item)->qi_next = NULL;					\
	if ((q)->dq_head == NULL) {				\
		(item)->qi_prev = NULL;				\
		(q)->dq_head = (item);				\
	} else {						\
		(item)->qi_prev = (q)->dq_tail;			\
		(q)->dq_tail->qi_next = (item);			\
	}							\
	(q)->dq_tail = (item);					\
} while(0)

#define DQUEUE_ADD_HEAD(q, item)				\
do {								\
	(item)->qi_prev = NULL;					\
	if ((q)->dq_head == NULL) {				\
		(item)->qi_next = NULL;				\
		(q)->dq_tail = (item);				\
	} else {						\
		(item)->qi_next = (q)->dq_head;			\
		(q)->dq_head->qi_prev = (item);			\
	}							\
	(q)->dq_head = (item);					\
} while(0)

#define DQUEUE_HEAD(q) 						\
	((q)->dq_head)

#define DQUEUE_TAIL(q) 						\
	((q)->dq_tail)

#define DQUEUE_POP_HEAD(q)					\
do {								\
	(q)->dq_head = (q)->dq_head->qi_next;			\
	if ((q)->dq_head != NULL)				\
		(q)->dq_head->qi_prev = NULL;			\
} while(0)

#define DQUEUE_POP_TAIL(q)					\
do {								\
	(q)->dq_tail = (q)->dq_tail->qi_prev;			\
	if ((q)->dq_tail != NULL)				\
		(q)->dq_tail->qi_next = NULL;			\
} while(0)

#define DQUEUE_EMPTY(q)						\
	((q)->dq_head == NULL)

#define DQUEUE_NEXT(it)						\
	(it)->qi_next

#define DQUEUE_PREV(it)						\
	(it)->qi_prev

#define DQUEUE_FOREACH(q, it, TYPE)				\
	for (TYPE* (it) = DQUEUE_HEAD(q);			\
	     (it) != NULL;					\
	     (it) = DQUEUE_NEXT(it))

#define DQUEUE_FOREACH_SAFE(q, it, TYPE)			\
	for (TYPE *(it) = DQUEUE_HEAD(q),			\
	     *__it = DQUEUE_NEXT(it);				\
	     (it) != NULL;					\
	     (it) = __it,					\
	     __it = (__it != NULL) ? DQUEUE_NEXT(__it) : NULL)

#define DQUEUE_FOREACH_REVERSE(q, it, TYPE)			\
	for (TYPE* (it) = DQUEUE_TAIL(q);			\
	     (it) != NULL;					\
	     (it) = DQUEUE_PREV(it))

#define DQUEUE_FOREACH_REVERSE_SAFE(q, it, TYPE)		\
	for (TYPE *(it) = DQUEUE_TAIL(q),			\
	     *__it = DQUEUE_PREV(it);				\
	     (it) != NULL;					\
	     (it) = __it,					\
	     __it = (__it != NULL) ? DQUEUE_PREV(__it) : NULL)

#define DQUEUE_REMOVE(q, it)					\
do {								\
	if ((it)->qi_prev != NULL)				\
		(it)->qi_prev->qi_next = (it)->qi_next;		\
	if ((it)->qi_next != NULL)				\
		(it)->qi_next->qi_prev = (it)->qi_prev;		\
	if ((q)->dq_head == (it)) 				\
		(q)->dq_head = (it)->qi_next;			\
	if ((q)->dq_tail == (it)) 				\
		(q)->dq_tail = (it)->qi_prev;			\
} while(0)

#endif /* __ANANAS_DQUEUE_H__ */

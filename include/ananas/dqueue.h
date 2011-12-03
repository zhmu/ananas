#ifndef __ANANAS_DQUEUE_H__
#define __ANANAS_DQUEUE_H__

/*
 * A dqueue is a doubly-linked structure; obtaining/removing the head takes
 * O(1), adding a new item takes O(1) and removing any item takes O(1).
 * Iterating through the entire queue takes O(n) (this also holds for locating
 * a single item).
 *
 * Each queue has a 'head' and 'tail' element, and every item has a previous
 * and next pointer. Usage instructions can be found in the 'dqueue' structure
 * test.
 *
 * The _IP-suffixed macro's allow a type-specific item prefix (ip) to be given;
 * this is useful when an item is to be placed in multiple queues. This is rare,
 * so the normal macro's simply expand to the item prefix macro's with the 'qi'
 * prefix for 'queue item'.
 */

#define DQUEUE_FIELDS_IT(TYPE, ip)				\
	TYPE* ip ## _prev;					\
	TYPE* ip ## _next;

#define DQUEUE_FIELDS(type)					\
	DQUEUE_FIELDS_IT(type, qi)

#define DQUEUE_DEFINE_BEGIN(name, TYPE)				\
	struct name {						\
		TYPE* dq_head;					\
		TYPE* dq_tail;

#define DQUEUE_DEFINE_END					\
	};

#define DQUEUE_DEFINE(name, TYPE)				\
	DQUEUE_DEFINE_BEGIN(name, TYPE)				\
	DQUEUE_DEFINE_END

#define DQUEUE_INIT(q) 						\
do {								\
	(q)->dq_head = NULL;					\
	(q)->dq_tail = NULL;					\
} while(0)

#define DQUEUE_ADD_TAIL_IP(q, ip, item)				\
do {								\
	(item)->ip ## _next = NULL;				\
	if ((q)->dq_head == NULL) {				\
		(item)->ip ## _prev = NULL;			\
		(q)->dq_head = (item);				\
	} else {						\
		(item)->ip ## _prev = (q)->dq_tail;		\
		(q)->dq_tail->ip ## _next = (item);		\
	}							\
	(q)->dq_tail = (item);					\
} while(0)

#define DQUEUE_ADD_HEAD_IP(q, ip, item)				\
do {								\
	(item)->ip ## _prev = NULL;				\
	if ((q)->dq_head == NULL) {				\
		(item)->ip ## _next = NULL;			\
		(q)->dq_tail = (item);				\
	} else {						\
		(item)->ip ## _next = (q)->dq_head;		\
		(q)->dq_head->ip ## _prev = (item);		\
	}							\
	(q)->dq_head = (item);					\
} while(0)

#define DQUEUE_HEAD(q) 						\
	((q)->dq_head)

#define DQUEUE_TAIL(q) 						\
	((q)->dq_tail)

#define DQUEUE_POP_HEAD_IP(q, ip)				\
do {								\
	(q)->dq_head = (q)->dq_head->ip ## _next;		\
	if ((q)->dq_head != NULL)				\
		(q)->dq_head->ip ## _prev = NULL;		\
} while(0)

#define DQUEUE_POP_TAIL_IP(q, ip)				\
do {								\
	(q)->dq_tail = (q)->dq_tail->ip ## _prev;		\
	if ((q)->dq_tail != NULL)				\
		(q)->dq_tail->ip ## _next = NULL;		\
} while(0)

#define DQUEUE_EMPTY(q)						\
	((q)->dq_head == NULL)

#define DQUEUE_NEXT_IP(it, ip)					\
	(it)->ip ## _next

#define DQUEUE_PREV_IP(it, ip)					\
	(it)->ip ## _prev

#define DQUEUE_FOREACH_IP(q, ip, it, TYPE)			\
	for (TYPE* (it) = DQUEUE_HEAD(q);			\
	     (it) != NULL;					\
	     (it) = DQUEUE_NEXT_IP(it, ip))

#define DQUEUE_FOREACH_SAFE_IP(q, ip, it, TYPE)			\
	for (TYPE *(it) = DQUEUE_HEAD(q),			\
	     *__it = DQUEUE_NEXT_IP(it, ip);			\
	     (it) != NULL;					\
	     (it) = __it,					\
	     __it = (__it != NULL) ? DQUEUE_NEXT_IP(__it, ip) : NULL)

#define DQUEUE_FOREACH_REVERSE_IP(q, ip, it, TYPE)		\
	for (TYPE* (it) = DQUEUE_TAIL(q);			\
	     (it) != NULL;					\
	     (it) = DQUEUE_PREV_IP(it, ip))

#define DQUEUE_FOREACH_REVERSE_SAFE_IP(q, ip, it, TYPE)		\
	for (TYPE *(it) = DQUEUE_TAIL(q),			\
	     *__it = DQUEUE_PREV_IP(it, ip);			\
	     (it) != NULL;					\
	     (it) = __it,					\
	     __it = (__it != NULL) ? DQUEUE_PREV_IP(__it, ip) : NULL)

#define DQUEUE_REMOVE_IP(q, ip, it)					\
do {									\
	if ((it)->ip ## _prev != NULL)					\
		(it)->ip ## _prev->ip ## _next = (it)->ip ## _next;	\
	if ((it)->ip ## _next != NULL)					\
		(it)->ip ## _next->ip ## _prev = (it)->ip ## _prev;	\
	if ((q)->dq_head == (it)) 					\
		(q)->dq_head = (it)->ip ## _next;			\
	if ((q)->dq_tail == (it)) 					\
		(q)->dq_tail = (it)->ip ## _prev;			\
} while(0)

#define DQUEUE_ADD_TAIL(q, item)				\
	DQUEUE_ADD_TAIL_IP(q, qi, item)

#define DQUEUE_ADD_HEAD(q, item)				\
	DQUEUE_ADD_HEAD_IP(q, qi, item)

#define DQUEUE_POP_HEAD(q)					\
	DQUEUE_POP_HEAD_IP(q, qi)

#define DQUEUE_POP_TAIL(q)					\
	DQUEUE_POP_TAIL_IP(q, qi)

#define DQUEUE_NEXT(it)						\
	DQUEUE_NEXT_IP(it, qi)

#define DQUEUE_REMOVE(q, it)					\
	DQUEUE_REMOVE_IP(q, qi, it)

#define DQUEUE_PREV(it)						\
	DQUEUE_PREV_IP(it, qi)

#define DQUEUE_FOREACH(q, it, TYPE)				\
	DQUEUE_FOREACH_IP(q, qi, it, TYPE)

#define DQUEUE_FOREACH_SAFE(q, it, TYPE)			\
	DQUEUE_FOREACH_SAFE_IP(q, qi, it, TYPE)

#define DQUEUE_FOREACH_REVERSE(q, it, TYPE)			\
	DQUEUE_FOREACH_REVERSE_IP(q, qi, it, TYPE)

#define DQUEUE_FOREACH_REVERSE_SAFE(q, it, TYPE)		\
	DQUEUE_FOREACH_REVERSE_SAFE_IP(q, qi, it, TYPE)

#endif /* __ANANAS_DQUEUE_H__ */

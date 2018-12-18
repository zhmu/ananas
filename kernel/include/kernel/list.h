#ifndef __ANANAS_LIST_H__
#define __ANANAS_LIST_H__

/*
 * This implements a standard doubly-linked list structure; obtaining/removing
 * the head takes O(1), adding a new item takes O(1) and removing any item
 * takes O(1).  Iterating through the entire list takes O(n) (this also holds
 * for locating a single item).
 *
 * Each list has a 'head' and 'tail' element, and every item has a previous
 * and next pointer. Usage instructions can be found in the 'list' structure
 * test.
 *
 * The _IP-suffixed macro's allow a type-specific item prefix (ip) to be given;
 * this is useful when an item is to be placed in multiple lists. This is rare,
 * so the normal macro's simply expand to the item prefix macro's with the 'li'
 * prefix for 'list item'.
 */
#define LIST_FIELDS_IT(TYPE, ip) \
    TYPE* ip##_prev;             \
    TYPE* ip##_next;

#define LIST_FIELDS(type) LIST_FIELDS_IT(type, li)

#define LIST_DEFINE_BEGIN(name, TYPE) \
    struct name {                     \
        TYPE* l_head;                 \
        TYPE* l_tail;

#define LIST_DEFINE_END \
    }                   \
    ;

#define LIST_DEFINE(name, TYPE)   \
    LIST_DEFINE_BEGIN(name, TYPE) \
    LIST_DEFINE_END

/* Appends an item at the tail of the list - the new item is the last */
#define LIST_APPEND_IP(q, ip, item)          \
    do {                                     \
        (item)->ip##_next = NULL;            \
        if ((q)->l_head == NULL) {           \
            (item)->ip##_prev = NULL;        \
            (q)->l_head = (item);            \
        } else {                             \
            (item)->ip##_prev = (q)->l_tail; \
            (q)->l_tail->ip##_next = (item); \
        }                                    \
        (q)->l_tail = (item);                \
    } while (0)

/* Prepends an item at the head of the list - the new item is the first */
#define LIST_PREPEND_IP(q, ip, item)         \
    do {                                     \
        (item)->ip##_prev = NULL;            \
        if ((q)->l_head == NULL) {           \
            (item)->ip##_next = NULL;        \
            (q)->l_tail = (item);            \
        } else {                             \
            (item)->ip##_next = (q)->l_head; \
            (q)->l_head->ip##_prev = (item); \
        }                                    \
        (q)->l_head = (item);                \
    } while (0)

/* Removes the head item from the list */
#define LIST_POP_HEAD_IP(q, ip)               \
    do {                                      \
        (q)->l_head = (q)->l_head->ip##_next; \
        if ((q)->l_head != NULL)              \
            (q)->l_head->ip##_prev = NULL;    \
    } while (0)

/* Removes the tail item from the list */
#define LIST_POP_TAIL_IP(q, ip)               \
    do {                                      \
        (q)->l_tail = (q)->l_tail->ip##_prev; \
        if ((q)->l_tail != NULL)              \
            (q)->l_tail->ip##_next = NULL;    \
    } while (0)

#define LIST_NEXT_IP(it, ip) (it)->ip##_next

#define LIST_PREV_IP(it, ip) (it)->ip##_prev

#define LIST_INSERT_BEFORE_IP(q, ip, pos, item) \
    if ((pos)->ip##_prev != NULL)               \
        (pos)->ip##_prev->ip##_next = (item);   \
    (item)->ip##_next = (pos);                  \
    (item)->ip##_prev = (pos)->ip##_prev;       \
    (pos)->ip##_prev = (item);                  \
    if ((q)->l_head == (pos))                   \
        (q)->l_head = (item);

#define LIST_FOREACH_IP(q, ip, it, TYPE) \
    for (TYPE * (it) = LIST_HEAD(q); (it) != NULL; (it) = LIST_NEXT_IP(it, ip))

#define LIST_FOREACH_SAFE_IP(q, ip, it, TYPE)                                            \
    for (TYPE * (it) = LIST_HEAD(q), *__it = (it != NULL) ? LIST_NEXT_IP(it, ip) : NULL; \
         (it) != NULL; (it) = __it, __it = (__it != NULL) ? LIST_NEXT_IP(__it, ip) : NULL)

#define LIST_FOREACH_REVERSE_IP(q, ip, it, TYPE) \
    for (TYPE * (it) = LIST_TAIL(q); (it) != NULL; (it) = LIST_PREV_IP(it, ip))

#define LIST_FOREACH_REVERSE_SAFE_IP(q, ip, it, TYPE)                                    \
    for (TYPE * (it) = LIST_TAIL(q), *__it = (it != NULL) ? LIST_PREV_IP(it, ip) : NULL; \
         (it) != NULL; (it) = __it, __it = (__it != NULL) ? LIST_PREV_IP(__it, ip) : NULL)

#define LIST_REMOVE_IP(q, ip, it)                         \
    do {                                                  \
        if ((it)->ip##_prev != NULL)                      \
            (it)->ip##_prev->ip##_next = (it)->ip##_next; \
        if ((it)->ip##_next != NULL)                      \
            (it)->ip##_next->ip##_prev = (it)->ip##_prev; \
        if ((q)->l_head == (it))                          \
            (q)->l_head = (it)->ip##_next;                \
        if ((q)->l_tail == (it))                          \
            (q)->l_tail = (it)->ip##_prev;                \
    } while (0)

#define LIST_INIT(q)        \
    do {                    \
        (q)->l_head = NULL; \
        (q)->l_tail = NULL; \
    } while (0)

#define LIST_EMPTY(q) ((q)->l_head == NULL)

#define LIST_HEAD(q) ((q)->l_head)

#define LIST_TAIL(q) ((q)->l_tail)

#define LIST_APPEND(q, item) LIST_APPEND_IP(q, li, item)

#define LIST_PREPEND(q, item) LIST_PREPEND_IP(q, li, item)

#define LIST_POP_HEAD(q) LIST_POP_HEAD_IP(q, li)

#define LIST_POP_TAIL(q) LIST_POP_TAIL_IP(q, li)

#define LIST_NEXT(it) LIST_NEXT_IP(it, li)

#define LIST_REMOVE(q, it) LIST_REMOVE_IP(q, li, it)

#define LIST_PREV(it) LIST_PREV_IP(it, li)

#define LIST_INSERT_BEFORE(q, pos, item) LIST_INSERT_BEFORE_IP(q, li, pos, item)

#define LIST_FOREACH(q, it, TYPE) LIST_FOREACH_IP(q, li, it, TYPE)

#define LIST_FOREACH_SAFE(q, it, TYPE) LIST_FOREACH_SAFE_IP(q, li, it, TYPE)

#define LIST_FOREACH_REVERSE(q, it, TYPE) LIST_FOREACH_REVERSE_IP(q, li, it, TYPE)

#define LIST_FOREACH_REVERSE_SAFE(q, it, TYPE) LIST_FOREACH_REVERSE_SAFE_IP(q, li, it, TYPE)

#endif /* __ANANAS_LIST_H__ */

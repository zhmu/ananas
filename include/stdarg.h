#ifndef __VARARGS_H__
#define __VARARGS_H__

/*
 * Implements variable arguments; the idea behind this code is based on 
 * FreeBSD's implementation of va_list.
 */

typedef char* va_list;

/*
 * Returns the length of 'a', rounded up to the next int. An int is
 * used because an int generally represents register size, which is
 * how the arguments are passed on the stack...
 */
#define va_length(a) \
	(sizeof(a) + (sizeof(a) % sizeof(int) == 0 ? 0 : sizeof(int) - (sizeof(a) % sizeof(int))))

/* XXX this depends on GCC */
#define va_start(ap, last) \
	((ap) = (va_list)__builtin_next_arg(last))

/*
 * Returns the next argument, cast to type*. This works by first updating the
 * ap pointer to the next item, and subsequently updating subtracing this
 * address so that the current item (as of the call) is returned.
 */
#define va_arg(ap, type) \
	(*(type *)((ap) += va_length(type), (ap) - va_length(type)))

/* Included for completeness, not actually used */
#define va_end(ap)

#endif /* __VARARGS_H__ */

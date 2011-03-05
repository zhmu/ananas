#ifndef __ANANAS_CDEFS_H__
#define __ANANAS_CDEFS_H__

#ifndef __GNUC__
#error Only definitions for gcc are available
#endif

/* Marks a function as 'does not return' */
#define __noreturn __attribute__((noreturn))

/* Aligns a variable on a given boundary */
#define __align(x) __attribute__((align(x)))

/* Used to mark a malloc function, which returns unique non-NULL pointers */
#define __malloc __attribute__((malloc))

/* Used to indicate that arguments may not be NULL */
#define __nonnull __attribute((nonnull))

#endif /* __ANANAS_CDEFS_H__ */

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

/* Used to indicate that a value is unused */
#define __unused __attribute__((unused))

#ifndef static_assert
#define STATIC_ASSERT3(line, cond, msg) \
	typedef char __unused static_assert_failure_in_line_##line[(cond) ? 1 : -1]

#define STATIC_ASSERT2(line, cond, msg) \
	STATIC_ASSERT3(line, cond, msg)

#define static_assert(cond, msg) \
	STATIC_ASSERT2(__LINE__, (cond), (msg))
#endif

/* Creates a string of x => "x" */
#define __STRINGIFY2(x) #x
#define STRINGIFY(x) __STRINGIFY2(x)

/* Used in header files to play nice with C++ applications */
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

#endif /* __ANANAS_CDEFS_H__ */

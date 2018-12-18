#include <machine/_pthread_types.h>

#ifndef __PTHREAD_T_DEFINED
/*
 * We use pointers to everything to avoid any userland dependency on their size/layout; also, at
 * least some software (LLVM) depends on being able to compare these using < instead of
 * pthread_equal().
 */
typedef struct pthread_attr* pthread_attr_t;
typedef struct pthread_cond* pthread_cond_t;
typedef struct pthread_condattr* pthread_condattr_t;
typedef struct pthread_key* pthread_key_t;
typedef struct pthread_mutex* pthread_mutex_t;
typedef struct pthread_mutexattr* pthread_mutexattr_t;
typedef struct pthread_once* pthread_once_t;
typedef struct pthread_rwlock* pthread_rwlock_t;
typedef struct pthread_rwlockattr* pthread_rwlockattr_t;
typedef struct pthread* pthread_t;
#define __PTHREAD_T_DEFINED
#endif

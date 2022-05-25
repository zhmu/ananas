/* { dg-do run { target aarch64_sve_hw } } */
/* { dg-options "-Ofast -mlow-precision-div" } */

#include "recip_2.c"

#define N 77

#define TEST_LOOP(TYPE)					\
  {							\
    TYPE a[N], b[N];					\
    for (int i = 0; i < N; ++i)				\
      {							\
	a[i] = i + 11;					\
	b[i] = i + 1;					\
      }							\
    test_##TYPE (a, b, N);				\
    for (int i = 0; i < N; ++i)				\
      {							\
	double diff = a[i] - (i + 11.0) / (i + 1);	\
	if (__builtin_fabs (diff) > 0x1.0p-8)		\
	  __builtin_abort ();				\
      }							\
  }

int
main (void)
{
  TEST_ALL (TEST_LOOP);
  return 0;
}

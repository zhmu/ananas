#ifndef __MATH_H__
#define __MATH_H__

#include <sys/cdefs.h>

typedef float float_t;
typedef double double_t;

#define HUGE_VAL	__builtin_huge_val()

__BEGIN_DECLS

double fabs(double x);
float fabsf(float x);
double frexp(double num, int* exp);

__END_DECLS

#endif /* __MATH_H__ */

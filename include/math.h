#ifndef __MATH_H__
#define __MATH_H__

typedef float float_t;
typedef double double_t;

#define HUGE_VAL	__builtin_huge_val()

double fabs(double x);
float fabsf(float x);
double frexp(double num, int* exp);

#endif /* __MATH_H__ */

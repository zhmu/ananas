#ifndef __INTTYPES_H__
#define __INTTYPES_H__

#include <stdint.h>

typedef struct {
	intmax_t quot;
	intmax_t rem;
} imaxdiv_t;

intmax_t imaxabs(intmax_t);
imaxdiv_t imaxdiv(intmax_t, intmax_t);
intmax_t strtoimax(const char * nptr, char ** endptr, int base);
uintmax_t strtoumax(const char * nptr, char ** endptr, int base);

#endif /* __INTTYPES_H__ */

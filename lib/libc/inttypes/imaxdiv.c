#include <inttypes.h>

imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom)
{
	imaxdiv_t i;
	/* XXX this is inefficient */
	i.quot = numer / denom;
	i.rem = numer % denom;
	return i;
}

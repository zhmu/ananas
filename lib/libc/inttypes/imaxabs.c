#include <inttypes.h>

intmax_t imaxabs(intmax_t j)
{
	if (j >= 0)
		return j;
	return -j;
}

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void _PDCLIB_assert( char const * const message1, char const * const function, char const * const message2 )
{
	fprintf(stderr, "%s%s%s\n", message1, function, message2);
	abort();
}

/* vim:set ts=2 sw=2: */

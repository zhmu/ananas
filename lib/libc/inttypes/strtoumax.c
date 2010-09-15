#include <limits.h>
#include <stdlib.h>

#include <inttypes.h>

uintmax_t strtoumax( const char * s, char ** endptr, int base )
{
    unsigned long long int rc;
    char sign = '+';
    const char * p = _PDCLIB_strtox_prelim( s, &sign, &base );
    if ( base < 2 || base > 36 ) return 0;
    rc = _PDCLIB_strtox_main( &p, (unsigned)base, (uintmax_t)UINTMAX_MAX, (uintmax_t)( UINTMAX_MAX / base ), (int)( UINTMAX_MAX % base ), &sign );
    if ( endptr != NULL ) *endptr = ( p != NULL ) ? (char *) p : (char *) s;
    return ( sign == '+' ) ? rc : -rc;
}

#include <limits.h>
#include <stdlib.h>

#include <inttypes.h>

/* XXX this is a horrible hack to prevent a segfault; min is actually one less */
#undef LLONG_MIN
#define LLONG_MIN  -9223372036854775807

intmax_t strtoimax( const char * s, char ** endptr, int base )
{
    long long int rc;
    char sign = '+';
    const char * p = _PDCLIB_strtox_prelim( s, &sign, &base );
    if ( base < 2 || base > 36 ) return 0;
    if ( sign == '+' )
    {
        rc = (long long int)_PDCLIB_strtox_main( &p, (unsigned)base, (uintmax_t)LLONG_MAX, (uintmax_t)( LLONG_MAX / base ), (int)( LLONG_MAX % base ), &sign );
    }
    else
    {
        /* FIXME: This breaks on some machines that round negatives wrongly */
        /* FIXME: Sign error not caught by testdriver */
        rc = (long long int)_PDCLIB_strtox_main( &p, (unsigned)base, (uintmax_t)LLONG_MIN, (uintmax_t)( LLONG_MIN / -base ), (int)( -( LLONG_MIN % base ) ), &sign );
    }
    if ( endptr != NULL ) *endptr = ( p != NULL ) ? (char *) p : (char *) s;
    return ( sign == '+' ) ? rc : -rc;
}

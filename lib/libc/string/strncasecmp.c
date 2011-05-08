#include <ctype.h>
#include <string.h>

int strncasecmp( const char * s1, const char * s2, size_t n )
{
    while ( ( *s1 ) && n && ( toupper(*s1) == toupper(*s2) ) )
    {
        ++s1;
        ++s2;
	--n;
    }
    if (n == 0)
    {
	return 0;
    }
    return ( toupper(*(unsigned char *)s1) - toupper(*(unsigned char *)s2) );
}

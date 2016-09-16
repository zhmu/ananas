#include <ctype.h>
#include <string.h>

int strcasecmp( const char * s1, const char * s2 )
{
    while ( ( *s1 ) && ( toupper(*s1) == toupper(*s2) ) )
    {
        ++s1;
        ++s2;
    }
    return ( toupper(*(unsigned char *)s1) - toupper(*(unsigned char *)s2) );
}

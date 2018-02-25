/* wctype( const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <wctype.h>
#include <string.h>
#include "_PDCLIB_locale.h"

wctype_t wctype( const char * property )
{
    if(property) switch(property[0])
    {
        case 'a':
            if(strcmp(property, "alpha") == 0) {
                return _PDCLIB_CTYPE_ALPHA;
            } else if(strcmp(property, "alnum") == 0) {
                return _PDCLIB_CTYPE_ALPHA | _PDCLIB_CTYPE_DIGIT;
            } else return 0;

        case 'b':
            if(strcmp(property, "blank") == 0) {
                return _PDCLIB_CTYPE_BLANK;
            } else return 0;

        case 'c':
            if(strcmp(property, "cntrl") == 0) {
                return _PDCLIB_CTYPE_CNTRL;
            } else return 0;

        case 'd':
            if(strcmp(property, "digit") == 0) {
                return _PDCLIB_CTYPE_DIGIT;
            } else return 0;

        case 'g':
            if(strcmp(property, "graph") == 0) {
                return _PDCLIB_CTYPE_GRAPH;
            } else return 0;

        case 'l':
            if(strcmp(property, "lower") == 0) {
                return _PDCLIB_CTYPE_LOWER;
            } else return 0;

        case 'p':
            if(strcmp(property, "print") == 0) {
                return _PDCLIB_CTYPE_GRAPH | _PDCLIB_CTYPE_SPACE;
            } else if(strcmp(property, "punct") == 0) {
                return _PDCLIB_CTYPE_PUNCT;
            } else return 0;

        case 's':
            if(strcmp(property, "space") == 0) {
                return _PDCLIB_CTYPE_SPACE;
            } else return 0;

        case 'u':
            if(strcmp(property, "upper") == 0) {
                return _PDCLIB_CTYPE_UPPER;
            } else return 0;

        case 'x':
            if(strcmp(property, "xdigit") == 0) {
                return _PDCLIB_CTYPE_XDIGT;
            } else return 0;
    }
    return 0;
}

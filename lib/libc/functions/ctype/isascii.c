/* isascii( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <ctype.h>

int isascii(int c) { return c >= 0 && c <= 0177; }

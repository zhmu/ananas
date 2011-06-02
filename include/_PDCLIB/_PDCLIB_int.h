/* $Id: _PDCLIB_int.h 508 2010-12-30 22:43:20Z solar $ */

/* PDCLib internal integer logic <_PDCLIB_int.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

/* -------------------------------------------------------------------------- */
/* You should not have to edit anything in this file; if you DO have to, it   */
/* would be considered a bug / missing feature: notify the author(s).         */
/* -------------------------------------------------------------------------- */

#ifndef _PDCLIB_CONFIG_H
#define _PDCLIB_CONFIG_H _PDCLIB_CONFIG_H
#include <_PDCLIB/_PDCLIB_config.h>
#endif

#ifndef _PDCLIB_AUX_H
#define _PDCLIB_AUX_H _PDCLIB_AUX_H
#include <_PDCLIB/_PDCLIB_aux.h>
#endif

#include <machine/_stdint.h>
#include <machine/_types.h>
#include <stddef.h>
#include <limits.h>

/* null pointer constant */
#define _PDCLIB_NULL 0

/* -------------------------------------------------------------------------- */
/* Limits of native datatypes                                                 */
/* -------------------------------------------------------------------------- */
/* The definition of minimum limits for unsigned datatypes is done because    */
/* later on we will "construct" limits for other abstract types:              */
/* USHRT -> _PDCLIB_ + USHRT + _MIN -> _PDCLIB_USHRT_MIN -> 0                 */
/* INT -> _PDCLIB_ + INT + _MIN -> _PDCLIB_INT_MIN -> ... you get the idea.   */
/* -------------------------------------------------------------------------- */

/* Setting 'char' limits                                                      */
#define _PDCLIB_CHAR_BIT    CHAR_BIT
#define _PDCLIB_UCHAR_MIN   UCHAR_MIN
#define _PDCLIB_UCHAR_MAX   UCHAR_MAX
#define _PDCLIB_SCHAR_MIN   SCHAR_MIN
#define _PDCLIB_SCHAR_MAX   SCHAR_MAX
#define _PDCLIB_CHAR_MIN    CHAR_MIN
#define _PDCLIB_CHAR_MAX    CHAR_MAX

/* Setting 'short' limits                                                     */
#define _PDCLIB_SHRT_MAX      SHRT_MAX
#define _PDCLIB_SHRT_MIN      SHRT_MIN
#define _PDCLIB_USHRT_MAX     USHRT_MAX
#define _PDCLIB_USHRT_MIN     USHRT_MIN

/* Setting 'int' limits                                                       */
#define _PDCLIB_INT_MAX		INT_MAX
#define _PDCLIB_INT_MIN		INT_MIN
#define _PDCLIB_UINT_MAX	UINT_MAX
#define _PDCLIB_UINT_MIN	UINT_MIN

/* Setting 'long' limits                                                      */
#define _PDCLIB_LONG_MAX	LONG_MAX
#define _PDCLIB_LONG_MIN	LONG_MIN
#define _PDCLIB_ULONG_MAX	ULONG_MAX

/* Setting 'long long' limits                                                 */
#define _PDCLIB_LLONG_MAX	LLONG_MAX
#define _PDCLIB_LLONG_MIN	LLONG_MIN
#define _PDCLIB_ULLONG_MAX	ULLONG_MAX
#define _PDCLIB_ULLONG_MIN	ULLONG_MIN

/* -------------------------------------------------------------------------- */
/* <stdint.h> exact-width types and their limits                              */
/* -------------------------------------------------------------------------- */
/* Note that, for the "standard" widths of 8, 16, 32 and 64 bit, the "LEAST"  */
/* types are identical to the "exact-width" types, by definition.             */

/* Setting 'int8_t', its limits, its literal, and conversion macros.          */
typedef int8_t	_PDCLIB_int8_t;
typedef uint8_t	_PDCLIB_uint8_t;
#define _PDCLIB_INT8_MAX   CHAR_MAX
#define _PDCLIB_INT8_MIN   CHAR_MIN
#define _PDCLIB_UINT8_MAX  UCHAR_MAX
#if     CHAR_BIT == 8
#define _PDCLIB_8_CONV     hh
#else
#error Unsupported width of char (not 8 bits).
#endif

/* Setting 'int16_t', its limits, its literal, and conversion macros.         */
typedef int16_t		_PDCLIB_int16_t;
typedef uint16_t	_PDCLIB_uint16_t;
#define _PDCLIB_INT16_MAX  INT_MAX
#define _PDCLIB_INT16_MIN  INT_MIN
#define _PDCLIB_UINT16_MAX UINT_MAX
#if     UINT_MAX == 65535
#define _PDCLIB_16_CONV
#elif   USHRT_MAX == 65535
#define _PDCLIB_16_CONV    h
#else
#error Neither 'short' nor 'int' are 16-bit.
#endif

/* Setting 'int32_t', its limits, its literal, and conversion macros.         */
typedef int32_t		_PDCLIB_int32_t;
typedef uint32_t	_PDCLIB_uint32_t;
#define _PDCLIB_INT32_MAX  INT32_MAX
#define _PDCLIB_INT32_MIN  INT32_MIN
#define _PDCLIB_UINT32_MAX UINT32_MAX
#if     UINT32_MAX == 4294967295U
#define _PDCLIB_INT32_LITERAL
#define _PDCLIB_UINT32_LITERAL
#define _PDCLIB_32_CONV
#elif   ULONG_MAX == 4294967295U
#define _PDCLIB_INT32_LITERAL  l
#define _PDCLIB_UINT32_LITERAL ul
#define _PDCLIB_32_CONV        l
#else
#error Neither 'int' nor 'long' are 32-bit.
#endif

/* Setting 'int64_t', its limits, its literal, and conversion macros.         */
typedef int64_t		_PDCLIB_int64_t;
typedef uint64_t	_PDCLIB_uint64_t;
#define _PDCLIB_INT64_MAX  LONG_MAX
#define _PDCLIB_INT64_MIN  LONG_MIN
#define _PDCLIB_UINT64_MAX ULONG_MAX
#if     ULONG_MAX == 18446744073709551615ULL
#define _PDCLIB_INT64_LITERAL  l
#define _PDCLIB_UINT64_LITERAL ul
#define _PDCLIB_64_CONV        l
#elif ULLONG_MAX == 18446744073709551615ULL
#define _PDCLIB_INT64_LITERAL  ll
#define _PDCLIB_UINT64_LITERAL ull
#define _PDCLIB_64_CONV        ll
#else
#error Neither 'long' nor 'long long' are 64-bit.
#endif

/* -------------------------------------------------------------------------- */
/* <stdint.h> "fastest" types and their limits                                */
/* -------------------------------------------------------------------------- */
/* This is, admittedly, butt-ugly. But at least it's ugly where the average   */
/* user of PDCLib will never see it, and makes <_PDCLIB_config.h> much        */
/* cleaner.                                                                   */
/* -------------------------------------------------------------------------- */

typedef int_fast8_t _PDCLIB_int_fast8_t;
typedef uint_fast8_t PDCLIB_uint_fast8_t;
#define _PDCLIB_INT_FAST8_MIN	INT_FAST8_MIN
#define _PDCLIB_INT_FAST8_MAX	INT_FAST8_MAX
#define _PDCLIB_UINT_FAST8_MAX	UINT_FAST8_MAX

typedef int_fast16_t _PDCLIB_int_fast16_t;
typedef uint_fast16_t PDCLIB_uint_fast16_t;
#define _PDCLIB_INT_FAST16_MIN	INT_FAST16_MIN
#define _PDCLIB_INT_FAST16_MAX	INT_FAST16_MAX
#define _PDCLIB_UINT_FAST16_MAX	UINT_FAST16_MAX

typedef int_fast32_t _PDCLIB_int_fast32_t;
typedef uint_fast32_t PDCLIB_uint_fast32_t;
#define _PDCLIB_INT_FAST32_MIN  INT_FAST32_MIN
#define _PDCLIB_INT_FAST32_MAX 	INT_FAST32_MAX
#define _PDCLIB_UINT_FAST32_MAX	UINT_FAST32_MAX

typedef int_fast64_t _PDCLIB_int_fast64_t;
typedef uint_fast64_t PDCLIB_uint_fast64_t;
#define _PDCLIB_INT_FAST64_MIN  INT_FAST64_MIN
#define _PDCLIB_INT_FAST64_MAX 	INT_FAST64_MAX
#define _PDCLIB_UINT_FAST64_MAX	UINT_FAST64_MAX

/* -------------------------------------------------------------------------- */
/* Various <stddef.h> typedefs and limits                                     */
/* -------------------------------------------------------------------------- */

typedef ptrdiff_t _PDCLIB_ptrdiff_t;
#define _PDCLIB_PTRDIFF_MIN	PTRDIFF_MIN
#define _PDCLIB_PTRDIFF_MAX	PTRDIFF_MAX

#define _PDCLIB_SIG_ATOMIC_MIN SIG_ATOMIC_MIN
#define _PDCLIB_SIG_ATOMIC_MAX SIG_ATOMIC_MAX

typedef size_t	_PDCLIB_size_t;
#define _PDCLIB_SIZE_MAX	SIZE_MAX

typedef wchar_t			_PDCLIB_wchar_t;
#define _PDCLIB_WCHAR_MIN	WCHAR_MIN
#define _PDCLIB_WCHAR_MAX	WCHAR_MAX

typedef intptr_t	_PDCLIB_intptr_t;
typedef uintptr_t	_PDCLIB_uintptr_t;
#define _PDCLIB_INTPTR_MIN	INTPTR_MIN
#define _PDCLIB_INTPTR_MAX	INTPTR_MAX
#define _PDCLIB_UINTPTR_MAX	UINTPTR_MAX

typedef intmax_t		_PDCLIB_intmax_t;
typedef uintmax_t		_PDCLIB_uintmax_t;
#define _PDCLIB_INTMAX_MIN	INTMAX_MIN
#define _PDCLIB_INTMAX_MAX	INTMAX_MAX
#define _PDCLIB_UINTMAX_MAX	UINTMAX_MAX
#define _PDCLIB_INTMAX_C( value )	INTMAX_C( value )
#define _PDCLIB_UINTMAX_C( value )	UINTMAX_C( value )

/* -------------------------------------------------------------------------- */
/* Various <stdio.h> internals                                                */
/* -------------------------------------------------------------------------- */

/* Flags for representing mode (see fopen()). Note these must fit the same
   status field as the _IO?BF flags in <stdio.h> and the internal flags below.
*/
#define _PDCLIB_FREAD     8u
#define _PDCLIB_FWRITE   16u
#define _PDCLIB_FAPPEND  32u 
#define _PDCLIB_FRW      64u
#define _PDCLIB_FBIN    128u

/* Internal flags, made to fit the same status field as the flags above. */
/* -------------------------------------------------------------------------- */
/* free() the buffer memory on closing (false for user-supplied buffer) */
#define _PDCLIB_FREEBUFFER   512u
/* stream has encountered error / EOF */
#define _PDCLIB_ERRORFLAG   1024u
#define _PDCLIB_EOFFLAG     2048u
/* stream is wide-oriented */
#define _PDCLIB_WIDESTREAM  4096u
/* stream is byte-oriented */
#define _PDCLIB_BYTESTREAM  8192u
/* file associated with stream should be remove()d on closing (tmpfile()) */
#define _PDCLIB_DELONCLOSE 16384u
/* stream handle should not be free()d on close (stdin, stdout, stderr) */
#define _PDCLIB_STATIC     32768u

/* Position / status structure for getpos() / fsetpos(). */
struct _PDCLIB_fpos_t
{
    _PDCLIB_uint64_t offset; /* File position offset */
    int              status; /* Multibyte parsing state (unused, reserved) */
};

/* FILE structure */
struct _PDCLIB_file_t
{
    _PDCLIB_fd_t            handle;   /* OS file handle */
    char *                  buffer;   /* Pointer to buffer memory */
    _PDCLIB_size_t          bufsize;  /* Size of buffer */
    _PDCLIB_size_t          bufidx;   /* Index of current position in buffer */
    _PDCLIB_size_t          bufend;   /* Index of last pre-read character in buffer */
    struct _PDCLIB_fpos_t   pos;      /* Offset and multibyte parsing state */
    _PDCLIB_size_t          ungetidx; /* Number of ungetc()'ed characters */
    unsigned char *         ungetbuf; /* ungetc() buffer */
    unsigned int            status;   /* Status flags; see above */
    /* multibyte parsing status to be added later */
    char *                  filename; /* Name the current stream has been opened with */
    struct _PDCLIB_file_t * next;     /* Pointer to next struct (internal) */
};

/* -------------------------------------------------------------------------- */
/* Internal data types                                                        */
/* -------------------------------------------------------------------------- */

/* Structure required by both atexit() and exit() for handling atexit functions */
struct _PDCLIB_exitfunc_t
{
    struct _PDCLIB_exitfunc_t * next;
    void (*func)( void );
};

/* Structures required by malloc(), realloc(), and free(). */
struct _PDCLIB_headnode_t
{
    struct _PDCLIB_memnode_t * first;
    struct _PDCLIB_memnode_t * last;
};

struct _PDCLIB_memnode_t
{
    _PDCLIB_size_t size;
    struct _PDCLIB_memnode_t * next;
};

/* Status structure required by _PDCLIB_print(). */
struct _PDCLIB_status_t
{
    int              base;   /* base to which the value shall be converted   */
    _PDCLIB_int_fast32_t flags; /* flags and length modifiers                */
    _PDCLIB_size_t   n;      /* print: maximum characters to be written      */
                             /* scan:  number matched conversion specifiers  */
    _PDCLIB_size_t   i;      /* number of characters read/written            */
    _PDCLIB_size_t   current;/* chars read/written in the CURRENT conversion */
    char *           s;      /* *sprintf(): target buffer                    */
                             /* *sscanf():  source string                    */
    _PDCLIB_size_t   width;  /* specified field width                        */
    _PDCLIB_size_t   prec;   /* specified field precision                    */
    struct _PDCLIB_file_t * stream; /* *fprintf() / *fscanf() stream         */
    _PDCLIB_va_list  arg;    /* argument stack                               */
};

/* -------------------------------------------------------------------------- */
/* Declaration of helper functions (implemented in functions/_PDCLIB).        */
/* -------------------------------------------------------------------------- */

/* This is the main function called by atoi(), atol() and atoll().            */
_PDCLIB_intmax_t _PDCLIB_atomax( const char * s );

/* Two helper functions used by strtol(), strtoul() and long long variants.   */
const char * _PDCLIB_strtox_prelim( const char * p, char * sign, int * base );
_PDCLIB_uintmax_t _PDCLIB_strtox_main( const char ** p, unsigned int base, _PDCLIB_uintmax_t error, _PDCLIB_uintmax_t limval, int limdigit, char * sign );

/* Digits arrays used by various integer conversion functions */
extern char _PDCLIB_digits[];
extern char _PDCLIB_Xdigits[];

/* The worker for all printf() type of functions. The pointer spec should point
   to the introducing '%' of a conversion specifier. The status structure is to
   be that of the current printf() function, of which the members n, s, stream
   and arg will be preserved; i will be updated; and all others will be trashed
   by the function.
   Returns a pointer to the first character not parsed as conversion specifier.
*/
const char * _PDCLIB_print( const char * spec, struct _PDCLIB_status_t * status );

/* The worker for all scanf() type of functions. The pointer spec should point
   to the introducing '%' of a conversion specifier. The status structure is to
   be that of the current scanf() function, of which the member stream will be
   preserved; n, i, and s will be updated; and all others will be trashed by
   the function.
   Returns a pointer to the first character not parsed as conversion specifier,
   or NULL in case of error.
   FIXME: Should distinguish between matching and input error
*/
const char * _PDCLIB_scan( const char * spec, struct _PDCLIB_status_t * status );

/* Parsing any fopen() style filemode string into a number of flags. */
unsigned int _PDCLIB_filemode( const char * mode );

/* Sanity checking and preparing of read buffer, should be called first thing 
   by any stdio read-data function.
   Returns 0 on success, EOF on error.
   On error, EOF / error flags and errno are set appropriately.
*/
int _PDCLIB_prepread( struct _PDCLIB_file_t * stream );

/* Sanity checking, should be called first thing by any stdio write-data
   function.
   Returns 0 on success, EOF on error.
   On error, error flags and errno are set appropriately.
*/
int _PDCLIB_prepwrite( struct _PDCLIB_file_t * stream );

/* Closing all streams on program exit */
void _PDCLIB_closeall( void );

/* -------------------------------------------------------------------------- */
/* errno                                                                      */
/* -------------------------------------------------------------------------- */

/* If PDCLib would call its error number "errno" directly, there would be no way
   to catch its value from underlying system calls that also use it (i.e., POSIX
   operating systems). That is why we use an internal name, providing a means to
   access it through <errno.h>.
*/
extern int _PDCLIB_errno;

/* A mechanism for delayed evaluation. (Not sure if this is really necessary, so
   no detailed documentation on the "why".)
*/
int * _PDCLIB_errno_func( void );

/* -------------------------------------------------------------------------- */
/* <ctype.h> lookup tables                                                    */
/* -------------------------------------------------------------------------- */

#define _PDCLIB_CTYPE_ALPHA   1
#define _PDCLIB_CTYPE_BLANK   2
#define _PDCLIB_CTYPE_CNTRL   4
#define _PDCLIB_CTYPE_GRAPH   8
#define _PDCLIB_CTYPE_PUNCT  16
#define _PDCLIB_CTYPE_SPACE  32
#define _PDCLIB_CTYPE_LOWER  64
#define _PDCLIB_CTYPE_UPPER 128
#define _PDCLIB_CTYPE_DIGIT 256
#define _PDCLIB_CTYPE_XDIGT 512

struct _PDCLIB_ctype_t
{
    _PDCLIB_uint16_t flags;
    unsigned char upper;
    unsigned char lower;
    unsigned char collation;
};


/* atexit( void (*)( void ) )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>
#include "_PDCLIB_io.h"

/* TODO: 32 is guaranteed. This should be dynamic but ATM gives problems
   with my malloc XXX Check this!
 */
#define NUMBER_OF_SLOTS 40

struct atexit_func {
    enum at_type {
        AF_STD,
        AF_CXX,
        AF_Ignore,
    } at_type;
    union {
        void (*std_func)(void);
        void (*cxx_func)(void*);
    } at_func;
    void* at_arg;
    void* at_dso;
};

static struct atexit_func _PDCLIB_exitfunc[NUMBER_OF_SLOTS] = {
    {.at_type = AF_STD, .at_func.std_func = &_PDCLIB_closeall}};
static size_t _PDCLIB_exitptr = NUMBER_OF_SLOTS;

int atexit(void (*func)(void))
{
    if (_PDCLIB_exitptr == 0)
        return -1;

    struct atexit_func* at = &_PDCLIB_exitfunc[--_PDCLIB_exitptr];
    at->at_type = AF_STD;
    at->at_func.std_func = func;
    at->at_arg = NULL;
    at->at_dso = NULL;
    return 0;
}

int __cxa_atexit(void (*func)(void*), void* arg, void* dso)
{
    if (_PDCLIB_exitptr == 0)
        return -1;

    struct atexit_func* at = &_PDCLIB_exitfunc[--_PDCLIB_exitptr];
    at->at_type = AF_CXX;
    at->at_func.cxx_func = func;
    at->at_arg = arg;
    at->at_dso = dso;
    return 0;
}

void __cxa_finalize(void* dso)
{
    for (int n = _PDCLIB_exitptr; n < NUMBER_OF_SLOTS; n++) {
        struct atexit_func* at = &_PDCLIB_exitfunc[n];
        if (dso != NULL && (at->at_type != AF_CXX || dso != at->at_dso))
            continue;

        switch (at->at_type) {
            case AF_STD:
                at->at_func.std_func();
                break;
            case AF_CXX:
                at->at_func.cxx_func(at->at_arg);
                break;
            default:
                break;
        }
        at->at_type = AF_Ignore; /* do not call again */
    }
}

#ifdef TEST_TODO // TODO: how to test using gtest?
#include "_PDCLIB_test.h"
#include <assert.h>

static int flags[32];

static void counthandler(void)
{
    static int count = 0;
    flags[count] = count;
    ++count;
}

static void checkhandler(void)
{
    for (int i = 0; i < 31; ++i) {
        assert(flags[i] == i);
    }
}

int main(void)
{
    TESTCASE(atexit(&checkhandler) == 0);
    for (int i = 0; i < 31; ++i) {
        TESTCASE(atexit(&counthandler) == 0);
    }
    return TEST_RESULTS;
}

#endif

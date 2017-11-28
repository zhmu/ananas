#ifndef __amd64__
// We are based on musl's arch/x86_64/bits/alltypes.h.in
#error "Port me!"
#endif

#if defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 2
typedef long double float_t;
typedef long double double_t;
#else
typedef float float_t;
typedef double double_t;
#endif

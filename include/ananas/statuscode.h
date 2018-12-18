#ifndef ANANAS_STATUSCODE_H
#define ANANAS_STATUSCODE_H

#include <ananas/_types/statuscode.h>

// Extracting things from the statuscode_t provided by every systemcall
// on failure: bit 31 = set, and
// errno: bits  8..0  -> 0...511
// line:  bits 19..9  -> 0..2047
// file:  bits 30..20 -> 0..2047
// on success: bit 31 = clear
static inline unsigned int ananas_statuscode_extract_errno(statuscode_t n) { return n & 0x1ff; }

static inline unsigned int ananas_statuscode_extract_line(statuscode_t n)
{
    return (n >> 9) & 0x7ff;
}

static inline unsigned int ananas_statuscode_extract_fileid(statuscode_t n)
{
    return (n >> 20) & 0x1ff;
}

static inline unsigned int ananas_statuscode_extract_value(statuscode_t n)
{
    return n & 0x7fffffff;
}

static inline statuscode_t
ananas_statuscode_make_failure(unsigned int fileid, unsigned int line, unsigned int no)
{
    return (1 << 31) | (fileid << 20) | (line << 9) | no;
}

static inline int ananas_statuscode_is_success(unsigned int val) { return (val & 0x80000000) == 0; }

static inline int ananas_statuscode_is_failure(unsigned int val)
{
    return !ananas_statuscode_is_success(val);
}

static inline statuscode_t ananas_statuscode_make_success(unsigned int val)
{
    return val & 0x7fffffff;
}

#endif /* ANANAS_STATUSCODE_H */

#ifndef ANANAS_STATUSCODE_H
#define ANANAS_STATUSCODE_H

#include <ananas/_types/statuscode.h>

// Extracting things from the statuscode_t provided by every systemcall
// errno: bits  8..0  -> 0...511
// line:  bits 20..9  -> 0..4095
// file:  bits 31..21 -> 0..2047
static inline unsigned int ananas_statuscode_extract_errno(statuscode_t n) {
	return n & 0x1ff;
}

static inline unsigned int ananas_statuscode_extract_line(statuscode_t n) {
	return (n >> 9) & 0xfff;
}

static inline unsigned int ananas_statuscode_extract_fileid(statuscode_t n) {
	return (n >> 21) & 0x1ff;
}

static inline statuscode_t ananas_statuscode_make(unsigned int fileid, unsigned int file, unsigned int no) {
	return (fileid << 21) | (fileid << 9) | no;
}

static inline statuscode_t ananas_statuscode_success()
{
	return 0;
}

#endif /* ANANAS_STATUSCODE_H */

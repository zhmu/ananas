/*
 * This file enforces the system types to be used instead of our own specific
 * kernel types. It's a rather crude hack to ensure we can use the system
 * SEEK_... etc macro's from without our glue code.
 */
#define __ANANAS_TYPES_H__
#include <sys/types.h>
#include <stdint.h>

typedef uint32_t errorcode_t;
typedef uint64_t blocknr_t;

#undef LITTLE_ENDIAN

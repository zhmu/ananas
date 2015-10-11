#ifndef __ANANAS_ENDIAN_H__
#define __ANANAS_ENDIAN_H__

#include <machine/param.h> /* for ..._ENDIAN */

static inline uint16_t _swap16(uint16_t v)
{
	return (v >> 8) | (v & 0xff) << 8;
}

static inline uint32_t _swap32(uint32_t v)
{
	return (v >> 24) | ((v >> 16) & 0xff) << 8 | ((v >> 8) & 0xff) << 16 | (v & 0xff) << 24;
}

#ifdef BIG_ENDIAN
# define BIG_OR_LITTLE(be, le) (be)
#elif defined(LITTLE_ENDIAN)
# define BIG_OR_LITTLE(be, le) (le)
#else
# error "Endianness not set"
#endif

static inline uint16_t htole16(uint16_t v)
{
	return BIG_OR_LITTLE(_swap16(v), v);
}

static inline uint32_t htole32(uint16_t v)
{
	return BIG_OR_LITTLE(_swap32(v), v);
}

static inline uint32_t htobe16(uint16_t v)
{
	return BIG_OR_LITTLE(v, _swap16(v));
}

static inline uint32_t htobe32(uint16_t v)
{
	return BIG_OR_LITTLE(v, _swap32(v));
}

static inline uint16_t betoh16(uint16_t v)
{
	return BIG_OR_LITTLE(v, _swap16(v));
}

static inline uint32_t betoh32(uint32_t v)
{
	return BIG_OR_LITTLE(v, _swap32(v));
}

#endif /* __ANANAS_ENDIAN_H__ */

#ifndef __ANANAS_CBUFFER_H__
#define __ANANAS_CBUFFER_H__

/*
 * A circular buffer is a simple structure which consists of a fixed-sized
 * buffer and a read and write pointer. 
 *
 * There are three sitations we need to distinguish within the buffer:
 *
 * 1) The buffer is empty
 *    In this case, read_ptr == write_ptr. We must ensure this condition cannot
 *    become true after filling the buffer (i.e. it is not possible to use the
 *    full buffer itself, always minus one byte)
 *
 * 2) write_ptr > read_ptr
 *
 *    0                           n
 *    +---------------------------+
 *    |          XXXXXX|          +   X is data left to read
 *    +----------|-----|----------+
 *               |     ^
 *               r     w
 *
 * 3) read_ptr > write_ptr
 *
 *    0                           n
 *    +---------------------------+
 *    |XXXXXXXXXX|     XXXXXXXXXXX+   X is data left to read
 *    +----------|-----|----------+
 *               |     ^
 *               w     r
 *
 * This implementation solves the problems by simply working byte-for-byte;
 * this isn't the most efficient approach but it is the easiest. If this
 * proves to be a bottleneck in the future, this approach will need work.
 */

#define CBUFFER_DATA_LEFT(cb) \
	(((cb)->cb_read_ptr == (cb)->cb_write_ptr) ? 0 : \
	((((cb)->cb_read_ptr < (cb)->cb_write_ptr) ? \
	 (((cb)->cb_write_ptr) - (cb)->cb_read_ptr) : \
	 (((cb)->cb_buffer_size - (cb)->cb_read_ptr)) + (cb)->cb_write_ptr)))

#define CBUFFER_EMPTY(cb) \
	((cb)->cb_read_ptr == (cb)->cb_write_ptr)

#define CBUFFER_FIELDS						\
	void* cb_buffer;					\
	size_t cb_buffer_size;					\
	size_t cb_read_ptr;					\
	size_t cb_write_ptr

#define CBUFFER_DEFINE(name)					\
	struct name { CBUFFER_FIELDS; }

#define CBUFFER_INIT(cb,buf,len)				\
do {								\
	(cb)->cb_buffer = (void*)(buf);				\
	(cb)->cb_buffer_size = (len);				\
	(cb)->cb_read_ptr = 0;					\
	(cb)->cb_write_ptr = 0;					\
} while(0)

#define CBUFFER_WRITE(cb, data, len)				\
({								\
	size_t _l = 0;						\
	unsigned char* _buf = (unsigned char*)((cb)->cb_buffer);\
	unsigned char* _data = (unsigned char*)(data);		\
	for (size_t _n = 0; _n < (len); _n++, _l++, _data++) {	\
		if ((((cb)->cb_write_ptr + 1) % (cb)->cb_buffer_size) == ((cb)->cb_read_ptr)) \
			break;					\
		*(_buf + (cb)->cb_write_ptr) = *_data;		\
		(cb)->cb_write_ptr = ((cb)->cb_write_ptr + 1) % (cb)->cb_buffer_size; \
	}							\
	_l;							\
})

#define CBUFFER_READ(cb, data, len)				\
({								\
	size_t _l = 0;						\
	unsigned char* _buf = (unsigned char*)((cb)->cb_buffer);\
	unsigned char* _data = (unsigned char*)(data);		\
	for (size_t _n = 0; _n < (len); _n++, _l++, _data++) {	\
		if ((cb)->cb_read_ptr == (cb)->cb_write_ptr)	\
			break;					\
		*_data = *(_buf + (cb)->cb_read_ptr);		\
		(cb)->cb_read_ptr = ((cb)->cb_read_ptr + 1) % (cb)->cb_buffer_size; \
	}							\
	_l;							\
})


#endif /* __ANANAS_CBUFFER_H__ */

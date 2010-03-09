#ifndef __ASSERT_H__
#define __ASSERT_H__

void __assert(const char*, const char*, int, const char*);

#define assert(x) ((x) ? (void)0 : __assert(__func__, __FILE__, __LINE__, #x))

#endif /* __ASSERT_H__ */

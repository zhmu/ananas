#ifndef __STRINGS_H__
#define __STRINGS_H__

#include <ananas/types.h> /* for size_t */

int bcmp(const void*, const void*, size_t);
void bcopy(const void*, void*, size_t);
int ffs(int);
char* index(const char*, int);
char* rindex(const char*, int);
int strcasecmp(const char*, const char*);
int strncasecmp(const char*, const char*, size_t);

#endif /* __STRINGS_H__ */

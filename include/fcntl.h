#include <ananas/types.h>

#ifndef __FCNTL_H__
#define __FCNTL_H__

#ifndef __MODE_T_DEFINED
typedef uint16_t	mode_t;
#define __MODE_T_DEFINED
#endif

#include <ananas/flags.h>

int creat(const char*, mode_t);
int open(const char*, int, ...);
int fcntl(int fildes, int cmd, ...);

#endif /* __FCNTL_H__ */

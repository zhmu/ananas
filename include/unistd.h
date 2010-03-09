#include <machine/_types.h>

#ifndef __UNISTD_H___
#define __UNISTD_H__

ssize_t read(int d, void* buf, size_t len);
ssize_t write(int d, const void* buf, size_t len);

#endif /* __UNISTD_H__ */

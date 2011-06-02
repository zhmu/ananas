#include <machine/_types.h>
#include <machine/_stddef.h>

#ifndef __RESOURCE_H__
#define __RESOURCE_H__

void* map(size_t len);
int unmap(void* ptr, size_t len);

#endif /* __RESOURCE_H__ */

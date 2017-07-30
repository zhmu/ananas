#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include <machine/_types.h>
#include <machine/_stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

void* map(size_t len);
int unmap(void* ptr, size_t len);

__END_DECLS

#endif /* __RESOURCE_H__ */

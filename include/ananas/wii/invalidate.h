#ifndef __ANANAS_WII_INVALIDATE_H__
#define __ANANAS_WII_INVALIDATE_H__

#include <ananas/types.h>

void invalidate_for_read(void* p, uint32_t size);
void invalidate_after_write(void* p, uint32_t size);

#endif /* __ANANAS_WII_INVALIDATE_H__ */

/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include <sys/types.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

void* map(size_t len);
int unmap(void* ptr, size_t len);

__END_DECLS

#endif /* __RESOURCE_H__ */

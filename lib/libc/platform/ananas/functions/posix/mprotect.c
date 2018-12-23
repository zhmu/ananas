/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/statuscode.h>
#include <ananas/syscall-vmops.h>
#include <ananas/syscalls.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

int mprotect(void* addr, size_t len, int prot)
{
    struct VMOP_OPTIONS vo;

    memset(&vo, 0, sizeof(vo));
    vo.vo_size = sizeof(vo);
    vo.vo_op = OP_CHANGE_ACCESS;
    vo.vo_addr = addr;
    vo.vo_len = len;
    if (prot & PROT_READ)
        vo.vo_flags |= VMOP_FLAG_READ;
    if (prot & PROT_WRITE)
        vo.vo_flags |= VMOP_FLAG_WRITE;
    if (prot & PROT_EXEC)
        vo.vo_flags |= VMOP_FLAG_EXECUTE;

    statuscode_t status = sys_vmop(&vo);
    if (ananas_statuscode_is_failure(status)) {
        errno = ananas_statuscode_extract_errno(status);
        return -1;
    }

    return 0;
}

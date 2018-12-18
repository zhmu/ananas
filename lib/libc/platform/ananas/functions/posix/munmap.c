#include <ananas/types.h>
#include <ananas/syscall-vmops.h>
#include <ananas/syscalls.h>
#include <sys/mman.h>
#include <string.h>
#include "_map_statuscode.h"

int munmap(void* addr, size_t len)
{
    struct VMOP_OPTIONS vo;

    memset(&vo, 0, sizeof(vo));
    vo.vo_size = sizeof(vo);
    vo.vo_op = OP_UNMAP;
    vo.vo_addr = addr;
    vo.vo_len = len;
    statuscode_t status = sys_vmop(&vo);
    return map_statuscode(status);
}

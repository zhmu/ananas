#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscall-vmops.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <sys/mman.h>
#include <string.h>

int munmap(void* addr, size_t len)
{
	struct VMOP_OPTIONS vo;

	memset(&vo, 0, sizeof(vo));
	vo.vo_size = sizeof(vo);
	vo.vo_op = OP_UNMAP;
	vo.vo_addr = addr;
	vo.vo_len = len;
	errorcode_t err = sys_vmop(&vo);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	return 0;
}

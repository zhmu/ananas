#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscall-vmops.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <sys/mman.h>
#include <string.h>

void* mmap(void* ptr, size_t len, int prot, int flags, int fd, off_t offset)
{
	struct VMOP_OPTIONS vo;

	memset(&vo, 0, sizeof(vo));
	vo.vo_size = sizeof(vo);
	vo.vo_op = OP_MAP;
	vo.vo_addr = ptr;
	vo.vo_len = len;
	if (prot & PROT_READ)
		vo.vo_flags |= VMOP_FLAG_READ;
	if (prot & PROT_WRITE)
		vo.vo_flags |= VMOP_FLAG_WRITE;
	if (prot & PROT_EXEC)
		vo.vo_flags |= VMOP_FLAG_EXECUTE;
	if (flags & MAP_PRIVATE)
		vo.vo_flags |= VMOP_FLAG_PRIVATE;

	if (flags & MAP_ANONYMOUS) {
		vo.vo_handle = -1;
		vo.vo_offset = 0;
	} else {
		vo.vo_handle = fd;
		vo.vo_offset = offset;
	}
	
	errorcode_t err = sys_vmop(&vo);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return MAP_FAILED;
	}

	return vo.vo_addr;
}

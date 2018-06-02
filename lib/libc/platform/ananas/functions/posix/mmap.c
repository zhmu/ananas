#include <ananas/types.h>
#include <ananas/statuscode.h>
#include <ananas/syscall-vmops.h>
#include <ananas/syscalls.h>
#include <sys/mman.h>
#include <errno.h>
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
	if (flags & MAP_FIXED)
		vo.vo_flags |= VMOP_FLAG_FIXED;
	if (flags & MAP_PRIVATE)
		vo.vo_flags |= VMOP_FLAG_PRIVATE;
	else
		vo.vo_flags |= VMOP_FLAG_SHARED;

	if (flags & MAP_ANONYMOUS) {
		vo.vo_fd = -1;
		vo.vo_offset = 0;
	} else {
		vo.vo_flags |= VMOP_FLAG_FD;
		vo.vo_fd = fd;
		vo.vo_offset = offset;
	}

	statuscode_t status = sys_vmop(&vo);
	if (ananas_statuscode_is_failure(status)) {
		errno = ananas_statuscode_extract_errno(status);
		return MAP_FAILED;
	}

	return vo.vo_addr;
}

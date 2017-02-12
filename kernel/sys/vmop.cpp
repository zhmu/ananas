#include <ananas/types.h>
#include <ananas/syscall.h>
#include <ananas/syscalls.h>
#include <ananas/process.h>
#include <ananas/trace.h>
#include <ananas/error.h>
#include <ananas/vm.h>
#include <ananas/syscall-vmops.h>
#include <ananas/vmspace.h>

TRACE_SETUP;

static errorcode_t
sys_vmop_map(ARG_CURTHREAD struct VMOP_OPTIONS* vo)
{
	/* TODO: support handle-based mappings */
	if (vo->vo_flags & VMOP_FLAG_HANDLE)
		return ANANAS_ERROR(BAD_HANDLE);

	/* TODO: support different mappings */

	/* TODO: support fixed mappings? */
	if (vo->vo_addr != NULL)
		return ANANAS_ERROR(BAD_ADDRESS);
	if (vo->vo_len == 0)
		return ANANAS_ERROR(BAD_LENGTH);

	int vm_flags = VM_FLAG_USER | VM_FLAG_ALLOC;
	if (vo->vo_flags & VMOP_FLAG_READ)
		vm_flags |= VM_FLAG_READ;
	if (vo->vo_flags & VMOP_FLAG_WRITE)
		vm_flags |= VM_FLAG_WRITE;
	if (vo->vo_flags & VMOP_FLAG_EXECUTE)
		vm_flags |= VM_FLAG_EXECUTE;

	vmarea_t* va;
	errorcode_t err = vmspace_map(curthread->t_process->p_vmspace, (addr_t)NULL, vo->vo_len, vm_flags, &va);
	ANANAS_ERROR_RETURN(err);

	vo->vo_addr = (void*)va->va_virt;
	vo->vo_len = va->va_len;
	return ananas_success();
}

static errorcode_t
sys_vmop_unmap(ARG_CURTHREAD struct VMOP_OPTIONS* vo)
{
	/* XXX implement me */
	return ANANAS_ERROR(BAD_OPERATION);
}

errorcode_t
sys_vmop(ARG_CURTHREAD struct VMOP_OPTIONS* opts)
{
	TRACE(SYSCALL, FUNC, "t=%p, opts=%p", curthread, opts);
	errorcode_t err;

	/* Opbtain options */
	struct VMOP_OPTIONS* vmop_opts;
	err = syscall_map_buffer(curthread, opts, sizeof(*vmop_opts), VM_FLAG_READ | VM_FLAG_WRITE, (void**)&vmop_opts);
	ANANAS_ERROR_RETURN(err);
	if (vmop_opts->vo_size != sizeof(*vmop_opts))
		return ANANAS_ERROR(BAD_LENGTH);

	switch(vmop_opts->vo_op) {
		case OP_MAP:
			return sys_vmop_map(curthread, vmop_opts);
		case OP_UNMAP:
			return sys_vmop_unmap(curthread, vmop_opts);
		default:
			return ANANAS_ERROR(BAD_OPERATION);
	}

	/* NOTREACHED */
}

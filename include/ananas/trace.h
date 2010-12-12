/*
 * Ananas tracing facilities
 *
 * Every file using this must invoke 'TRACE_SETUP', which will set up the
 * appropriate tracing context. Then, TRACE_FILE_ID will be an unique ID
 * per file, in the range 1 ... <number of files> + 1.
 *
 * This works by generating two sections in the ELF file:
 * - .tracenames: contains every filename
 * - .traceids: contains pointers to each filename in .tracenames
 *
 * What happens is that the linker script places tracenames and traceids at the
 * very end of the kernel, but before __end. A build step extracts these
 * sections to a custom file, which gets parsed by a script.
 *
 * The idea is that we'll see .traceids as simply an addr_t array; the first
 * index corresponds to file ID 1, the second to file ID 2 etc.
 */
#ifndef __ANANAS_TRACE_H__
#define __ANANAS_TRACE_H__

#define TRACE_SETUP \
	static const char __trace_filename[]  __attribute__((section(".tracenames"))) = __FILE__; \
	static addr_t __trace_id  __attribute__((section(".traceids"))) = (addr_t)&__trace_filename;

#ifdef KERNEL
#define TRACE_FILE_ID \
	(((addr_t)&__trace_id - (addr_t)&__traceid_begin) / sizeof(addr_t) + 1)
#else
#define TRACE_FILE_ID 0
#endif

/* Available subsystem trace types */
#define TRACE_SUBSYSTEM_DEBUG   0			/* Plain debugging */
#define TRACE_SUBSYSTEM_VFS     1			/* VFS layer */
#define TRACE_SUBSYSTEM_THREAD	2			/* Threads framework */
#define TRACE_SUBSYSTEM_EXEC	3			/* Execution  */
#define TRACE_SUBSYSTEM_BIO	4			/* Block I/O layer */
#define TRACE_SUBSYSTEM_HANDLE	5			/* Handle framework */
#define TRACE_SUBSYSTEM_SYSCALL	6			/* System calls */
#define TRACE_SUBSYSTEM_LAST	(TRACE_SUBSYSTEM_SYSCALL)

/* Available tracelevels */
#define TRACE_LEVEL_FUNC	0x0001			/* Function call tracing */
#define TRACE_LEVEL_ERROR	0x0002			/* Error report */
#define TRACE_LEVEL_INFO	0x0004			/* Information */
#define TRACE_LEVEL_WARN	0x0008			/* Warning */

#define TRACE(s,l,x...)	 \
	if (trace_subsystem_mask[TRACE_SUBSYSTEM_##s] & TRACE_LEVEL_##l) \
		tracef(TRACE_FILE_ID, __func__, x)

#define TRACE_ENABLE(s,l) \
	trace_subsystem_mask[TRACE_SUBSYSTEM_##s] |= TRACE_LEVEL_##l

#define TRACE_DISABLE(s,l) \
	trace_subsystem_mask[TRACE_SUBSYSTEM_##s] &= ~TRACE_LEVEL_##l
	
		
extern void *__traceid_begin, *__traceid_end;
extern uint32_t trace_subsystem_mask[];
void tracef(int fileid, const char* func, const char* fmt, ...);

#endif /* __ANANAS_TRACE_H__ */

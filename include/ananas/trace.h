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

#define TRACE_FILE_ID \
	(((addr_t)&__trace_id - (addr_t)&__traceid_begin) / sizeof(addr_t) + 1)

extern void *__traceid_begin, *__traceid_end;

#endif /* __ANANAS_TRACE_H__ */

/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
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

#include <ananas/types.h>
#include <ananas/util/array.h>

#ifdef KERNEL
#define TRACE_SETUP                                                                          \
    static const char __trace_filename[] __attribute__((section(".tracenames"))) = __FILE__; \
    static addr_t __trace_id __attribute__((section(".traceids"))) = (addr_t)&__trace_filename;
#define TRACE_FILE_ID (((addr_t)&__trace_id - (addr_t)&__traceid_begin) / sizeof(addr_t) + 1)
#else
#define TRACE_FILE_ID 0
#endif

extern void *__traceid_begin, *__traceid_end;

namespace trace
{
    // Available subsystem trace types
    enum class SubSystem {
        DEBUG = 0,   /* Plain debugging */
        VFS = 1,     /* VFS layer */
        THREAD = 2,  /* Threads framework */
        EXEC = 3,    /* Execution  */
        BIO = 4,     /* Block I/O layer */
        HANDLE = 5,  /* Handle framework */
        SYSCALL = 6, /* System calls */
        MACHDEP = 7, /* Machine dependent */
        USB = 8,     /* USB stack */
        VM = 9,      /* VM */
        _Last = VM
    };

    // Available tracelevels
    namespace level
    {
        static constexpr int FUNC = 0x0001;  /* Function call tracing */
        static constexpr int ERROR = 0x0002; /* Error report */
        static constexpr int INFO = 0x0004;  /* Information */
        static constexpr int WARN = 0x0008;  /* Warning */
        static constexpr int ALL = 0xffff;   /* Everything */
    }                                        // namespace level

#define TRACE(SUBSYSTEM, LEVEL, EXPR...)                          \
    (IsEnabled(trace::SubSystem::SUBSYSTEM, trace::level::LEVEL)) \
        ? trace::detail::tracef(TRACE_FILE_ID, __func__, EXPR)    \
        : (void)0

    namespace detail
    {
        extern util::array<uint32_t, static_cast<int>(trace::SubSystem::_Last)> subsystem_mask;
        void tracef(int fileid, const char* func, const char* fmt, ...);
    } // namespace detail

    constexpr inline bool IsEnabled(SubSystem ss, int level)
    {
        return (detail::subsystem_mask[static_cast<int>(ss)] & level) != 0;
    }

    void Enable(SubSystem ss, int level);
    void Disable(SubSystem ss, int level);

} // namespace trace

#endif /* __ANANAS_TRACE_H__ */

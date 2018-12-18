#include <ananas/types.h>
#include "kernel/console.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

extern uint32_t x86_get_ms_since_boot(); // XXX x86 only

namespace trace
{
    namespace detail
    {
        static constexpr int BufSize = 256;

        util::array<uint32_t, static_cast<int>(trace::SubSystem::_Last)> subsystem_mask;

        void tracef(int fileid, const char* func, const char* fmt, ...)
        {
            char buf[BufSize];
            {
                Thread* curthread = PCPU_GET(curthread);
                const char* tname = curthread->t_name;
                const auto pid = curthread->t_process != NULL ? curthread->t_process->p_pid : -1;

                const auto timestamp = x86_get_ms_since_boot();
                snprintf(
                    buf, sizeof(buf), "[%4u.%03u] (%d:%s) %s: ", timestamp / 1000, timestamp % 1000,
                    static_cast<int>(pid), tname, func);

                va_list va;
                va_start(va, fmt);
                vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 2, fmt, va);
                buf[sizeof(buf) - 2] = '\0';
                strcat(buf, "\n");
                va_end(va);
            }

            console_putstring(buf);
        }

    } // namespace detail

    void Enable(SubSystem ss, int mask) { detail::subsystem_mask[static_cast<int>(ss)] |= mask; }

    void Disable(SubSystem ss, int mask) { detail::subsystem_mask[static_cast<int>(ss)] |= ~mask; }

} // namespace trace

/* vim:set ts=2 sw=2: */

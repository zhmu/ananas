/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/processgroup.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/lib.h"

TRACE_SETUP;

namespace process
{
    extern Mutex process_mtx; // also used to protect processgrouplist
    ProcessGroupList s_processgroups;

    namespace
    {
        pid_t cursid = 1;

        util::locked<ProcessGroup> CreateProcessGroup(Session& session, pid_t pgid)
        {
            auto pg = new ProcessGroup(session, pgid);
            pg->Lock();

            {
                MutexGuard g(process_mtx);
                s_processgroups.push_back(*pg);
            }
            return util::locked<ProcessGroup>(*pg);
        }

        void DetachFromCurrentProcessGroup(Process& process)
        {
            if (process.p_group == nullptr)
                return; // nothing to detach from

            ProcessGroup& pg = *process.p_group;
            process.p_group = nullptr;

            {
                pg.Lock();
                pg.pg_members.remove(process);
                pg.Unlock();
            }
            pg.RemoveReference();
        }

    } // unnamed namespace

    Session::Session(pid_t sid) : s_sid(sid) {}

    ProcessGroup::ProcessGroup(Session& session, pid_t pgid) : pg_session(session), pg_id(pgid)
    {
        pg_session.AddReference();
    }

    ProcessGroup::~ProcessGroup()
    {
        {
            MutexGuard g(process_mtx);
            s_processgroups.remove(*this);
        }
        pg_session.RemoveReference();
    }

    Session* AllocateSession(Process& process)
    {
        const pid_t newSessionID = [] {
            MutexGuard g(process_mtx);
            return cursid++;
        }();

        auto session = new Session(newSessionID);
        session->s_leader = &process;
        return session;
    }

    util::locked<ProcessGroup> FindProcessGroupByID(pid_t pgid)
    {
        MutexGuard g(process_mtx);
        for (auto& pg : s_processgroups) {
            if (pg.pg_id != pgid)
                continue;
            pg.Lock();
            return util::locked<ProcessGroup>(pg);
        }

        return util::locked<ProcessGroup>();
    }

    void InitializeProcessGroup(Process& process, Process* parent)
    {
        auto pg = [&]() {
            if (parent != nullptr) {
                // Inherit the group of parent
                auto pg = parent->p_group;
                pg->AddReference();
                pg->Lock();
                return util::locked<ProcessGroup>(*pg);
            } else {
                // Create new session with a single process group
                auto session = process::AllocateSession(process);
                auto pg = CreateProcessGroup(*session, process.p_pid);
                session->RemoveReference();
                return pg;
            }
        }();

        pg->pg_members.push_back(process);

        // XXX lock process
        process.p_group = &*pg;
        pg.Unlock();
    }

    void AbandonProcessGroup(Process& process) { DetachFromCurrentProcessGroup(process); }

    void SetProcessGroup(Process& process, util::locked<ProcessGroup>& new_pg)
    {
        process.AssertLocked();
        if (process.p_group == &*new_pg)
            return; // nothing to change

        AbandonProcessGroup(process);
        new_pg->pg_members.push_back(process);
        process.p_group = &*new_pg;
    }

} // namespace process

#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/processgroup.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/lib.h"

TRACE_SETUP;

namespace process {

extern Mutex process_mtx; // also used to protect processgrouplist
ProcessGroupList s_processgroups;

namespace {
pid_t cursid = 1;

ProcessGroup* CreateProcessGroupAndLock(Session& session, pid_t pgid)
{
	auto pg = new ProcessGroup(session, pgid);
	pg->pg_mutex.Lock();

	{
		MutexGuard g(process_mtx);
		s_processgroups.push_back(*pg);
	}
	return pg;
}

void DetachFromCurrentProcessGroup(Process& process)
{
	if (process.p_group == nullptr)
		return; // nothing to detach from

	ProcessGroup& pg = *process.p_group;
	process.p_group = nullptr;

	const bool mustDestroyProcessGroup = [&]() {
		MutexGuard g(pg.pg_mutex);
		pg.pg_members.remove(process);
		return pg.pg_members.empty();
	}();

	if (!mustDestroyProcessGroup)
		return;

	auto session = pg.pg_session;
	const bool mustDestroySession = [&]() {
		MutexGuard g(session->s_mutex);
		session->s_num_groups--;
		return session->s_num_groups == 0;
	}();
	if (!mustDestroySession)
		return;

	delete session;
}

} // unnamed namespace

Session* AllocateSession()
{
	const pid_t newSessionID = []{
		MutexGuard g(process_mtx);
		return cursid++;
	}();

	auto session = new Session;
	session->s_sid = newSessionID;
	session->s_num_groups = 0;
	return session;
}

ProcessGroup* FindProcessGroupByIDAndLock(pid_t pgid)
{
	MutexGuard g(process_mtx);
	for(auto& pg: s_processgroups) {
		if (pg.pg_id != pgid)
			continue;
		pg.pg_mutex.Lock();
		return &pg;
	}

	return nullptr;
}


Result AssignToProcessGroup(Process& process, Session& session, pid_t pgid)
{
	KASSERT(process.p_group != nullptr, "process %d without pgid", process.p_pid);
	if (process.p_group->pg_session != &session)
		return RESULT_MAKE_FAILURE(EPERM); // cannot move across sessions

	// XXX This does implement any of the checks described in POSIX
	auto pg = FindProcessGroupByIDAndLock(pgid);
	if (pg == nullptr) {
		// Not found; create a new one XXX
		pg = CreateProcessGroupAndLock(session, pgid);
	}
	DetachFromCurrentProcessGroup(process);
	pg->pg_members.push_back(process);
	{
		auto session = pg->pg_session;
		MutexGuard g(session->s_mutex);
		session->s_num_groups++;
	}

	pg->pg_mutex.Unlock();
	return Result::Success();
}

void
InitializeProcessGroup(Process& process, Process* parent)
{
	auto lockedPg = [&]() {
		if (parent != nullptr) {
			// Just re-use process group of parent
			auto pg = parent->p_group;
			pg->pg_mutex.Lock();
			return pg;
		} else {
			// Create new session with a single process group
			auto session = process::AllocateSession();
			return CreateProcessGroupAndLock(*session, process.p_pid);
		}
	}();

	{
		auto session = lockedPg->pg_session;
		MutexGuard g(session->s_mutex);
		session->s_num_groups++;
	}
	lockedPg->pg_members.push_back(process);
	lockedPg->pg_mutex.Unlock();
	process.p_group = lockedPg;
}

void
AbandonProcessGroup(Process& process)
{
	DetachFromCurrentProcessGroup(process);
}

} // namespace process

/* vim:set ts=2 sw=2: */

#ifndef __PROCESSGROUP_H__
#define __PROCESSGROUP_H__

#include "kernel/process.h"
#include <ananas/util/refcounted.h>

class TTY;

namespace process {

struct Session : util::refcounted<Session>
{
	Session(pid_t sid);
	~Session() = default;
	Mutex		s_mutex{"session"};
	Process*	s_leader = nullptr;
	TTY*		s_control_tty = nullptr;
	const pid_t	s_sid;
};

struct ProcessGroup : util::List<ProcessGroup>::NodePtr, util::refcounted<ProcessGroup>
{
	ProcessGroup(Session& session, pid_t pgid);
	~ProcessGroup();

	void Lock()
	{
		pg_mutex.Lock();
	}

	void Unlock()
	{
		pg_mutex.Unlock();
	}

	void AssertLocked()
	{
		pg_mutex.AssertLocked();
	}

	pid_t		pg_id;
	Session&	pg_session;
	GroupMemberList	pg_members;

private:
	Mutex		pg_mutex{"pg"};
};
typedef util::List<ProcessGroup> ProcessGroupList;

void InitializeProcessGroup(Process& process, Process* parent);
void AbandonProcessGroup(Process& process);
util::locked<ProcessGroup> FindProcessGroupByID(pid_t pgid);
void SetProcessGroup(Process& process, util::locked<ProcessGroup>& new_pg);

} // namespace process

#endif /* __PROCESSGROUP_H__ */

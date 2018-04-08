#ifndef __PROCESSGROUP_H__
#define __PROCESSGROUP_H__

#include "kernel/process.h"

namespace process {

struct Session
{
	Mutex		s_mutex{"session"};
	int		s_num_groups;
	Process*	s_leader;
	pid_t		s_sid;
};

struct ProcessGroup : util::List<ProcessGroup>::NodePtr
{
	ProcessGroup(Session& session, pid_t pgid)
	 : pg_session(&session), pg_id(pgid)
	{
	}

	Mutex		pg_mutex{"pg"};
	pid_t		pg_id;
	Session*	pg_session;
	GroupMemberList	pg_members;
};
typedef util::List<ProcessGroup> ProcessGroupList;

void InitializeProcessGroup(Process& process, Process* parent);
void AbandonProcessGroup(Process& process);
ProcessGroup* FindProcessGroupByIDAndLock(pid_t pgid);
Result AssignToProcessGroup(Process& process, Session& session, pid_t pgid);

} // namespace process

#endif /* __PROCESSGROUP_H__ */

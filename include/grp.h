#ifndef __GRP_H__
#define __GRP_H__

#include <ananas/_types/uid.h>
#include <ananas/_types/gid.h>

struct group {
	char*	gr_name;
	gid_t	gr_gid;
	char**	gr_mem;
};

struct group* getgrgid(gid_t);
struct group* getgrnam(const char*);
void endgrent(void);
struct group* getgrent(void);
void setgrent(void);

#endif /* __GRP_H__ */

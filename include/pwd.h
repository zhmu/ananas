#ifndef __PWD_H__
#define __PWD_H__

#include <sys/_types/uid.h>
#include <sys/_types/gid.h>

struct passwd {
	char*	pw_name;
	uid_t	pw_uid;
	gid_t	pw_gid;
	char*	pw_dir;
	char*	pw_shell;
	/* extensions below here */
	char*	pw_passwd;
};

struct passwd* getpwnam(const char*);
struct passwd* getpwuid(uid_t);
void endpwent(void);
struct passwd* getpwent(void);
void setpwent(void);

#endif /* __PWD_H__ */

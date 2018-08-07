#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <machine/_types.h>
#include <ananas/_types/off.h>
#include <ananas/_types/gid.h>
#include <ananas/_types/size.h>
#include <ananas/_types/ssize.h>
#include <ananas/_types/pid.h>
#include <ananas/_types/uid.h>
#include <sys/cdefs.h>

/* XXX We lie and claim to support IEEE Std 1003.1-2001 */
#define _POSIX_VERSION 200112L

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

#ifndef SEEK_SET
#define SEEK_SET	0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR	1
#endif
#ifndef SEEK_END
#define SEEK_END	2
#endif

__BEGIN_DECLS

void	_exit(int status);
ssize_t read(int fd, void* buf, size_t len);
ssize_t write(int fd, const void* buf, size_t len);
off_t	lseek(int fd, off_t offset, int whence);
pid_t	fork(void);
int	close(int filedes);
int	dup(int filedes);
int	dup2(int filedes, int filedes2);
int	access(const char* path, int amode);
uid_t	getuid(void);
pid_t	getpid(void);
pid_t	getppid(void);
gid_t	getgid(void);
uid_t	geteuid(void);
gid_t	getegid(void);
int	getgroups(int gidsetsize, gid_t grouplist[]);
int	fsync(int fildes);
int	link(const char* path1, const char* path2);
int	chdir(const char* path);
int	fchdir(int fildes);
int	fchown(int fildes, uid_t owner, gid_t group);
int	isatty(int fildes);
int	ftruncate(int fildes, off_t length);
int	rmdir(const char* path);
char*	getlogin(void);
unsigned alarm(unsigned seconds);
int	setpgid(pid_t pid, pid_t pgid);
char*	ttyname(int fildes);
int	setgid(gid_t gid);
int	setuid(uid_t uid);
char*	crypt(const char* key, const char* salt);
int	unlink(const char* path);
int	chown(const char* path, uid_t owner, gid_t group);
unsigned sleep(unsigned seconds);

int	setsid();
int	setpgid(pid_t pid, pid_t pgid);
pid_t	getpgrp();

char*	getcwd(char* buf, size_t size);
ssize_t	readlink(const char* path, char* buf, size_t bufsize);

int	execvp(const char *path, char *const argv[]);
int	execv(const char *path, char *const argv[]);
int	execve(const char *path, char *const argv[], char *const envp[]);
int	execl(const char* path, const char* arg0, ...);
int	execlp(const char* path, const char* arg0, ...);

#define	R_OK 4
#define	W_OK 2
#define	X_OK 1
#define F_OK 8

extern int optind, opterr, optopt;
extern int optreset; /* XXX appears to be a bsd extension */
extern char* optarg;

extern char** environ;

#define _SC_PAGESIZE	1000
#define _SC_CLK_TCK	1001
long	sysconf(int name);

/* legacy interfaces - should be nuked sometime */
int getdtablesize();

int pipe(int fildes[2]);

pid_t tcgetpgrp(int fildes);
int tcsetpgrp(int fildes, pid_t pgid_id);

int gethostname(char* name, size_t namelen);

__END_DECLS

#endif /* __UNISTD_H__ */

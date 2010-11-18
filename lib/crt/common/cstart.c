#define NULL 0L

extern int main(int argc, char** argv, char** envp);
extern void libc_init(void* treadinfo);
extern char** environ;

extern int libc_argc;
extern char** libc_argv;

int
cstart(void* threadinfo)
{
	libc_init(threadinfo);
	return(main(libc_argc, libc_argv, environ));
}

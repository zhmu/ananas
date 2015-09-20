#include <sys/utsname.h>
#include <string.h>

int uname(struct utsname* utsname)
{
	/* XXX */
	strcpy(utsname->sysname, "Ananas");
	strcpy(utsname->nodename, "local");
	strcpy(utsname->release, "testing");
#ifdef __i386__
	strcpy(utsname->machine, "i386");
#elif defined(__amd64__)
	strcpy(utsname->machine, "amd64");
#else
	/* how did you get this to compile anyway? :-) */
	strcpy(utsname->machine, "unknown");
#endif
	strcpy(utsname->version, "0.1");
	return 0;
}

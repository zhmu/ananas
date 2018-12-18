#include <unistd.h>
#include <sys/tty.h>
#include <sys/ioctl.h>

int tcsetpgrp(int fildes, pid_t pgid) { return ioctl(fildes, TIOCSPGRP, pgid); }

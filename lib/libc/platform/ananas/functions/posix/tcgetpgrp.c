#include <unistd.h>
#include <sys/tty.h>
#include <sys/ioctl.h>

pid_t tcgetpgrp(int fildes) { return ioctl(fildes, TIOCGPGRP); }

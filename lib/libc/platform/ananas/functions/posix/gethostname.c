#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>

int gethostname(char* name, size_t namelen)
{
    int fd = open("/ankh/network/hostname", O_RDONLY);
    if (fd < 0)
        return -1;

    int n = read(fd, name, namelen);
    if (n >= 0)
        name[n] = '\0';

    close(fd);
    return n >= 0;
}

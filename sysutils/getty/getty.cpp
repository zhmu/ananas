/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * Ananas getty(8)
 *
 * This will open a terminal, set up the properties and launch a shell. No
 * logins yet!
 */
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <ananas/tty.h>
#include <sys/ioctl.h>
#include <cstring>
#include "common/paths.h"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " device\n";
        return EXIT_FAILURE;
    }

    // Open the TTY
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("cannot open tty");
        return EXIT_FAILURE;
    }

    // Use the new TTY for std{in,out,err}
    if (dup2(fd, STDIN_FILENO) < 0 || dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
        perror("dup2");
        return EXIT_FAILURE;
    }
    close(fd);

    // Start session and become controlling terminal
    if (setsid() < 0)
        perror("setsid");
    if (ioctl(STDIN_FILENO, TIOCSCTTY) < 0)
        perror("ioctl");

    // OK, now run the shell (or login later on)
    {
        char* const argv[] = {strdup("-sh"), nullptr};
        exit(execv(paths::sh.c_str(), argv));
    }

    return 0;
}

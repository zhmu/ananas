#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    // Our LDD works by simply setting the environment variable LD_LDD=1 and
    // invoking the executable.
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " file\n";
        return EXIT_FAILURE;
    }

    if (access(argv[1], X_OK) < 0) {
        std::cerr << argv[0] << ": cannot execute '" << argv[1] << "'\n";
        return EXIT_FAILURE;
    }

    {
        char* const exec_argv[] = {strdup(argv[1]), nullptr};
        char* const exec_envp[] = {strdup("LD_LDD=1"), nullptr};
        exit(execve(argv[1], exec_argv, exec_envp));
    }
}

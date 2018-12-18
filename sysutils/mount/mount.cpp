#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>

namespace
{
    constexpr const char* no_device = "none";

    void show_mounts()
    {
        struct statfs* mntbuf;
        int entries = getmntinfo(&mntbuf, 0);
        if (entries < 0) {
            perror("getmntinfo");
            return;
        }

        for (int n = 0; n < entries; n++) {
            const auto& sfs = mntbuf[n];
            if (sfs.f_mntfromname[0] != '\0')
                std::cout << sfs.f_mntfromname;
            else
                std::cout << no_device;
            std::cout << " on " << sfs.f_mntonname << " (" << sfs.f_fstypename;
            if (sfs.f_flags & MNT_RDONLY)
                std::cout << "ro";
            else
                std::cout << "rw";
            std::cout << ")\n";
        }
    }

    bool do_mount(const char* type, const char* from, const char* to)
    {
        if (strcmp(from, no_device) == 0)
            from = nullptr;
        if (mount(type, from, to, 0) == 0)
            return true;

        perror("mount");
        return false;
    }

} // unnamed namespace

int main(int argc, char* argv[])
{
    switch (argc) {
        case 1:
            show_mounts();
            return EXIT_SUCCESS;
        case 5:
            if (strcmp(argv[1], "-t") != 0) {
                std::cerr << "expected -t type\n";
                return EXIT_FAILURE;
            }
            return do_mount(argv[2], argv[3], argv[4]) ? EXIT_SUCCESS : EXIT_FAILURE;
        default:
            std::cerr << "usage: " << argv[0] << "\n";
            std::cerr << "       " << argv[0] << " -t type source dest\n";
            return EXIT_FAILURE;
    }

    // NOTREACHED
}

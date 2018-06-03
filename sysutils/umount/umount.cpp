#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>

namespace {

bool
do_umount(const char* path)
{
	if (unmount(path, 0) == 0)
		return true;

	perror("unmount");
	return false;
}

} // unnamed namespace

int
main(int argc, char* argv[])
{
	switch(argc) {
		case 2:
			return do_umount(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
		default:
			std::cerr << "usage: " << argv[0] << " path\n";
			return EXIT_FAILURE;
	}

	// NOTREACHED
}

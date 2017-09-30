// SUMMARY:Child inherits mappings
// PROVIDE-FILE: "mmap-5.txt" "ABCD"

#include "framework.h"
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

TEST_BODY_BEGIN
{
	int fd = open("mmap-5.txt", O_RDONLY);
	ASSERT_NE(-1, fd);

	void* p = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
	ASSERT_NE(MAP_FAILED, p);

	int pid = fork();

	// Have both child and parent check the mapping
	char* c = (char*)p;
	int n = 0;
	for (/* nothing */; n < 4; n++, c++) {
		ASSERT_EQ(*c, n + 'A');
	}

	if (pid == 0)
		exit(0);

	int stat;
	wait(&stat);

	// We expect the child to exit gracefully
	EXPECT_NE(0, WIFEXITED(stat));
	EXPECT_EQ(0, WEXITSTATUS(stat));
}
TEST_BODY_END

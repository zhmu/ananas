/*
 * Ananas system init(8)
 *
 * This expects to be started by the kernel as process id 1 and that
 * std{in,out,err} all points to /dev/console. We will, in sequence:
 *
 *   [rc] Run /etc/rc
 * [ttys] Start terminals as outlined in /etc/ttys and re-start them if they
 *        terminate
 * [wait] Wait for any process to terminate and kill it
 *
 * If the parent of any process terminates, it will become a child of init(8),
 * which is what the final step is for.
 *
 */
#include <sys/types.h>
#include <sys/wait.h>

#include <fstream>
#include <iostream>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "common/paths.h"
#include "common/util.h"
#include "ttys.h"

namespace {

std::vector<TTY> ttys;

bool
ParseTTYs()
{
	std::ifstream ifs(paths::ttys, std::ifstream::in);
	for(std::string s; std::getline(ifs, s); /* nothing */) {
		if (s.empty() || s[0] == '#')
			continue;

		TTY m;
		if (!m.Parse(s))
			continue;

		ttys.push_back(m);
	}

	return !ttys.empty();
}

void
runRcScript()
{
	// If the rc file does not exist, do not bother at all
	if (access(paths::rc.c_str(), R_OK) < 0 && errno == ENOENT)
		return;

	pid_t p = fork();
	if (p == 0) {
		// XXX for now, we'll explicitly ask /bin/sh to invoke rc for us...
		char* const argv[] = {
			strdup(paths::sh.c_str()),
			strdup(paths::rc.c_str()),
			nullptr
		};
		exit(execv(paths::sh.c_str(), argv));
	}

	int wstatus;
	if (wait(&wstatus) < 0)
		return; // XXX what can we do here?

	if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0)
		return; // all is okay

	std::cerr << paths::rc << " exited uncleanly!\n";
	// TODO go to single user mode?
}

} // unnamed namespace

int
main()
{
	// Do not terminate ourselves once things go
	signal(SIGCHLD, SIG_IGN);

	// [rc]
	runRcScript();

	// [ttys]
	if (!ParseTTYs()) {
		std::cerr << "no entries found in " << paths::ttys << " - assuming no ttys wanted\n";
	}

	while(true) {
		// See if all our TTY's are still okay
		for(auto& tty: ttys)
			tty.Poll();

		// Wait until stuff terminates
		wait(nullptr);
	}

	return 0;
}

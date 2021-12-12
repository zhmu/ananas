#include <catch2/catch_test_macros.hpp>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

TEST_CASE("waitpid() handles normal termination", "[wait]")
{
    constexpr auto exitCode = 42;
    auto pid = fork();
    if (pid == 0) {
        exit(exitCode);
    }
    int status;
    REQUIRE(waitpid(pid, &status, 0) == pid);

    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == exitCode);
}


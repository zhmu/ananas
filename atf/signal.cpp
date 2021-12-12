#include <catch2/catch_test_macros.hpp>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

TEST_CASE("SIGURG is ignored by default", "[signal]")
{
    auto pid = getpid();
    REQUIRE(kill(pid, SIGURG) == 0);
}

namespace
{
    int signals_caught;
    int last_caught;
    void CatchSignal(int num)
    {
        ++signals_caught;
        last_caught = num;
    }

    struct CatchSignalFixture
    {
        CatchSignalFixture() { signals_caught = 0; last_caught = -1; }
    };
}

TEST_CASE_METHOD(CatchSignalFixture, "SIGURG can be caught", "[signal]")
{
    signal(SIGURG, CatchSignal);
    auto pid = getpid();
    CHECK(kill(pid, SIGURG) == 0);
    CHECK(signals_caught == 1);
    CHECK(last_caught == SIGURG);
    signal(SIGURG, SIG_DFL);
}

TEST_CASE("SIGTERM terminates", "[signal]")
{
    auto pid = fork();
    if (pid == 0) {
        sleep(3);
        exit(1);
    }
    REQUIRE(kill(pid, SIGTERM) == 0);
    int status;
    REQUIRE(waitpid(pid, &status, 0) == pid);

    CHECK(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == SIGTERM);
}

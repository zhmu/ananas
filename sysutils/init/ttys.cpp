#include "ttys.h"
#include <vector>
#include <iostream>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "common/paths.h"
#include "common/util.h"

namespace
{
    constexpr time_t minRespawnTime = 3;

    enum class Field {
        Device,
        Command,
        TerminalType,
        Flags,
        COUNT // must be last
    };

} // unnamed namespace

bool TTY::Parse(const std::string& line)
{
    auto fields = util::Split(line);
    if (fields.size() != static_cast<size_t>(Field::COUNT)) {
        std::cerr << "tty line '" << line << "' has incorrect number of fields"
                  << "\n";
        return false;
    }

    mt_Flags = 0;
    const auto flags =
        util::Split(fields[static_cast<size_t>(Field::Flags)], [](char ch) { return ch == ','; });
    for (const auto& flag : flags) {
        if (flag == "on") {
            mt_Flags = mt_Flag_On;
        } else if (flag == "off") {
            mt_Flags &= ~mt_Flag_On;
        } else {
            std::cerr << "tty line '" << line << "' has unexpected flag '" << flag << "'\n";
            return false;
        }
    }

    mt_Device = fields[static_cast<size_t>(Field::Device)];
    mt_Command = [&]() {
        auto command = fields[static_cast<size_t>(Field::Command)];
        // Remove surrounding quotes of command
        if (!command.empty() && command[0] == '"' && command[command.size() - 1] == '"') {
            command = command.substr(1, command.size() - 2);
        }
        return util::Split(command);
    }();
    mt_Type = fields[static_cast<size_t>(Field::TerminalType)];
    return true;
}

void TTY::Poll()
{
    if ((mt_Flags & mt_Flag_On) == 0)
        return; // we are not enabled

    if (mt_Pid > 0 && kill(mt_Pid, 0) == 0)
        return; // still alive, nothing to do

    std::time_t currentTime = std::time(nullptr);
    if (mt_LastStarted >= 0 && mt_LastStarted + minRespawnTime > currentTime) {
        // Whoa, this process does not really last - give up
        std::cerr << "terminal " << mt_Device << " terminates too quickly, disabling!\n";
        mt_Flags &= ~mt_Flag_On;
        return;
    }

    mt_LastStarted = currentTime;
    pid_t p = fork();
    if (p == 0) {
        char** argv = new char*[mt_Command.size() + 1 /* device */ + 1 /* nullptr */];
        for (size_t n = 0; n < mt_Command.size(); n++) {
            argv[n] = strdup(mt_Command[n].c_str());
        }
        argv[mt_Command.size()] = strdup((paths::dev + mt_Device).c_str());
        argv[mt_Command.size() + 1] = nullptr;

        exit(execv(argv[0], argv));
    }

    mt_Pid = p;
}

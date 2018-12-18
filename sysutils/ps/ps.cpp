/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
// http://pubs.opengroup.org/onlinepubs/009604499/utilities/ps.html
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <iomanip>
#include "common/paths.h"
#include "common/util.h"

namespace
{
    int stringToInt(const std::string& s, int defaultValue = 0)
    {
        size_t idx;
        const auto v = std::stoi(s, &idx);
        return idx == s.size() ? v : defaultValue;
    }

    int getField(const std::vector<std::string>& fields, size_t index, int defaultValue)
    {
        if (index >= fields.size())
            return defaultValue;
        return stringToInt(fields[index], defaultValue);
    }

    std::string readLineFromFile(const std::string& path)
    {
        std::ifstream ifs(path, std::ifstream::in);
        std::string s;
        std::getline(ifs, s);
        return s;
    }

} // namespace

struct Process {
    Process(int pid, std::string&& name, const std::string& state)
        : p_pid(pid), p_name(std::move(name))
    {
        const auto stateFields = util::Split(state);
        p_state = getField(stateFields, 0, -1);
        p_ppid = getField(stateFields, 1, -1);
    }

    int p_pid = 0;
    int p_ppid = 0;
    int p_state = -1;
    std::string p_name;
};

bool GetProcesses(std::vector<Process>& processes)
{
    DIR* dir = opendir(paths::proc.c_str());
    if (dir == nullptr)
        return false;

    while (struct dirent* de = readdir(dir)) {
        const auto pid = stringToInt(de->d_name, 0);
        if (pid <= 0)
            continue; // not a valid pid

        const std::string procPath = paths::proc + "/" + std::to_string(pid);
        auto name = readLineFromFile(procPath + "/name");
        auto state = readLineFromFile(procPath + "/state");
        processes.push_back(Process(pid, std::move(name), state));
    }

    closedir(dir);
    return true;
}

int main(int argc, char* argv[])
{
    std::vector<Process> processes;
    if (!GetProcesses(processes)) {
        std::cerr << "unable to get process list\n";
        return EXIT_FAILURE;
    }

    std::cout << "  PID  PPID  ST CMD\n";
    for (auto& p : processes) {
        std::cout << std::setw(5) << std::setfill(' ') << p.p_pid << " " << std::setw(5)
                  << std::setfill(' ') << p.p_ppid << " " << std::setw(3) << std::setfill(' ')
                  << p.p_state << " " << p.p_name << "\n";
    }

    // TTY TIME CMD
    return EXIT_SUCCESS;
}

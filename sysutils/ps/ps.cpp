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

    unsigned int clockTicksPerSecond = 0;
} // namespace

struct Process {
    Process(int pid, std::string&& name, const std::string& status)
        : p_pid(pid), p_name(std::move(name))
    {
        const auto statusFields = util::Split(status);
        p_state = getField(statusFields, 0, -1);
        p_ppid = getField(statusFields, 1, -1);
        p_utime = getField(statusFields, 2, 0);
        p_stime = getField(statusFields, 3, 0);
        p_cutime = getField(statusFields, 4, 0);
        p_cstime = getField(statusFields, 5, 0);
    }

    int p_pid = 0;
    int p_ppid = 0;
    int p_state = -1;
    int p_utime{};
    int p_stime{};
    int p_cutime{};
    int p_cstime{};
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
        auto status = readLineFromFile(procPath + "/status");
        processes.push_back(Process(pid, std::move(name), status));
    }

    closedir(dir);
    return true;
}

namespace
{
    std::string FormatTime(const Process& p)
    {
        const auto totalTicks = p.p_utime + p.p_stime;
        const auto totalSeconds = totalTicks / clockTicksPerSecond;

        std::ostringstream ss;
        ss << (totalSeconds / 60) << ":" << std::setw(2) << std::setfill('0') << (totalSeconds % 60);
        return ss.str();
    }
}

int main(int argc, char* argv[])
{
    std::vector<Process> processes;
    if (!GetProcesses(processes)) {
        std::cerr << "unable to get process list\n";
        return EXIT_FAILURE;
    }

    clockTicksPerSecond = sysconf(_SC_CLK_TCK);

    std::cout << "  PID  PPID  ST TIME CMD\n";
    for (auto& p : processes) {
        std::cout << std::setw(5) << std::setfill(' ') << p.p_pid << " " << std::setw(5)
                  << std::setfill(' ') << p.p_ppid << " " << std::setw(3) << std::setfill(' ')
                  << p.p_state << " " << std::setw(5) << std::setfill(' ')
                  << FormatTime(p) << " " << p.p_name << "\n";
    }

    // TTY TIME CMD
    return EXIT_SUCCESS;
}

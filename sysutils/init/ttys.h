#pragma once

#include <sys/types.h>
#include <ctime>
#include <string>
#include <vector>

class TTY
{
  public:
    TTY() = default;

    bool Parse(const std::string& line);
    void Poll();

  private:
    std::string mt_Device;
    std::vector<std::string> mt_Command;
    std::string mt_Type;
    int mt_Flags = 0;

    static constexpr int mt_Flag_On = 1;

    pid_t mt_Pid = -1;
    std::time_t mt_LastStarted = -1;
};

/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <fstream>
#include <iostream>
#include <optional>
#include <map>
#include <string>
#include <unistd.h>
#include "common/paths.h"
#include "common/util.h"

namespace {
    using ValueType = uint64_t;
    using KeyValueMap = std::map<std::string, ValueType>;

    auto ParseMemoryFile()
    {
        KeyValueMap result;

        std::ifstream ifs(paths::memory, std::ifstream::in);
        for (std::string s; std::getline(ifs, s); ) {
            const auto f = util::Split(s);
            if (f.size() != 2) continue;
            result.insert({ f[0], std::stoi(f[1]) });
        }
        return result;
    }

    std::optional<ValueType> GetValue(const KeyValueMap& map, std::string_view sv)
    {
        const auto it = map.find(std::string(sv));
        if (it == map.end()) return {};
        return it->second;
    }
}

int main(int argc, char* argv[])
{
    const auto memory = ParseMemoryFile();
    const auto page_to_kb = sysconf(_SC_PAGE_SIZE) / 1024;

    const auto total_pages = GetValue(memory, "total_pages");
    const auto available_pages = GetValue(memory, "available_pages");
    if (total_pages && available_pages) {
        std::cout << "total memory    : " << (*total_pages * page_to_kb) << " kb\n";
        std::cout << "available memory: " << (*available_pages * page_to_kb) << " kb\n";
        const auto in_use = static_cast<int>(((*total_pages - *available_pages) / static_cast<float>(*total_pages)) * 100.0f);
        std::cout << "memory in use   : " << in_use << " %\n";
    } else {
        std::cout << "missing total_pages / available_pages in " << paths::memory << '\n';
    }
}

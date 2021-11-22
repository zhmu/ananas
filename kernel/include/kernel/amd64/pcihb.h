/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstdint>
#include "kernel-md/io.h"
#include "kernel/lib.h"

/* We use PCI configuration mechanism 1 as that is most commonly supported */

namespace pci {

namespace cfg1 {
    namespace io {
        static constexpr inline uint32_t Address = 0xcf8;
        static constexpr inline uint32_t Data = 0xcfc;
    }
    namespace addr {
        static constexpr inline uint32_t Enable = (1 << 31);
    }
}

using BusNo = uint32_t;
using DeviceNo = uint32_t;
using FuncNo = uint32_t;
using RegisterNo = uint32_t;

struct Identifier
{
    BusNo bus;
    DeviceNo device;
    FuncNo func;
};

namespace detail {
    static inline unsigned int MakeAddress(const Identifier& id, const RegisterNo reg)
    {
        return cfg1::addr::Enable | (id.bus << 16) | (id.device << 11) | (id.func << 8) | reg;
    }

    template<size_t Width> uint32_t ReadData();
    template<> inline uint32_t ReadData<4>() { return inl(cfg1::io::Data); }
    template<> inline uint32_t ReadData<2>() { return inw(cfg1::io::Data); }
    template<> inline uint32_t ReadData<1>() { return inb(cfg1::io::Data); }

    template<size_t Width> void WriteData(const uint32_t value);
    template<> inline void WriteData<4>(const uint32_t value) { outl(cfg1::io::Data, value); }
    template<> inline void WriteData<2>(const uint32_t value) { outw(cfg1::io::Data, value); }
    template<> inline void WriteData<1>(const uint32_t value) { outb(cfg1::io::Data, value); }
}

template<typename T> static inline T
ReadConfig(const Identifier& id, const RegisterNo reg)
{
    outl(cfg1::io::Address, detail::MakeAddress(id, reg));
    return static_cast<T>(detail::ReadData<sizeof(T)>());
}

template<typename T> static inline void
WriteConfig(const Identifier& id, const RegisterNo reg, const T value)
{
    outl(cfg1::io::Address, detail::MakeAddress(id, reg));
    detail::WriteData<sizeof(T)>(value);
}

}

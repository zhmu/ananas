/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2020 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"

// This file contains functionality to satisfy the C++ Itanium ABI
// (https://itanium-cxx-abi.github.io/cxx-abi/abi.html)
extern "C"
{
    void* __dso_handle = nullptr;

    void __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle)
    {
    }

    void __cxa_pure_virtual() { panic("pure virtual call"); }
    void __cxa_deleted_virtual() { panic("deleted virtual call"); }

    int __cxa_guard_acquire(__int64_t* guard_object)
    {
        // XXX We ought to actually lock stuff here
        auto initialized = reinterpret_cast<char*>(guard_object);
        return *initialized == 0;
    }

    void __cxa_guard_release(__int64_t* guard_object)
    {
        // XXX We ought to actually lock stuff here
        *guard_object = 0;
    }
}

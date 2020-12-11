/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/init.h"

struct DEntry;
class Result;
class Thread;
class VMSpace;

struct IExecutor {
    virtual Result Verify(DEntry& dentry) = 0;
    virtual Result Load(VMSpace& vs, DEntry& dentry, void*& auxargs) = 0;
    virtual Result PrepareForExecute(
        VMSpace& vs, Thread& t, void* auxargs, const char* argv[], const char* envp[]) = 0;
};

/*
 * Define an executable format.
 */
struct ExecFormat : util::List<ExecFormat>::NodePtr {
    ExecFormat(IExecutor& executor) : ef_executor(executor) {}

    /* Function handling the execution */
    IExecutor& ef_executor;
};

void exec_register_format(ExecFormat& ef);
IExecutor* exec_prepare(DEntry& dentry);

template<typename T>
struct RegisterExecutableFormat : init::OnInit {
    RegisterExecutableFormat()
        : OnInit(init::SubSystem::Thread, init::Order::Middle, []() {
              static T executor;
              static ExecFormat execFormat(executor);
              exec_register_format(execFormat);
          })
    {
    }
};

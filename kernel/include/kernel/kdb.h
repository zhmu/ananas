/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_KDB_H__
#define __ANANAS_KDB_H__

#include <ananas/util/list.h>
#include "kernel/init.h"

namespace kdb
{
    enum KDB_ARG_TYPE {
        T_INVALID, /* internal use only */
        T_STRING,
        T_NUMBER,
        T_BOOL
    };
    typedef enum KDB_ARG_TYPE kdb_arg_type_t;

    /*
     * An argument as passed to the command's backing function.
     */
    struct Argument {
        kdb_arg_type_t a_type;
        union {
            const char* u_string;
            unsigned long u_value;
        } a_u;
    };

    namespace detail
    {
        /* Callback function for a given command */
        typedef void kdb_func_t(int num_args, const Argument args[]);

        /* KDB command itself */
        struct KDBCommand : util::List<KDBCommand>::NodePtr {
            KDBCommand(const char* cmd, const char* args, const char* help, kdb_func_t* func)
                : cmd_command(cmd), cmd_args(args), cmd_help(help), cmd_func(func)
            {
            }
            const char* cmd_command;
            const char* cmd_args;
            const char* cmd_help;
            kdb_func_t* cmd_func;
        };

        void AddCommand(KDBCommand& cmd);

    } // namespace detail

    struct RegisterCommand {
        template<typename Func>
        RegisterCommand(const char* cmd, const char* args, const char* help, Func fn)
        {
            static detail::KDBCommand c(cmd, args, help, fn);
            detail::AddCommand(c);
        }

        template<typename Func>
        RegisterCommand(const char* cmd, const char* help, Func fn)
        {
            static detail::KDBCommand c(cmd, nullptr, help, fn);
            detail::AddCommand(c);
        }
    };

    void Enter(const char* why);
    void OnPanic();

} // namespace kdb

#endif /* __ANANAS_KDB_H__ */

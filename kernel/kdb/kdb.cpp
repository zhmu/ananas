/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include <ananas/types.h>
#include "kernel/console.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "options.h"
#ifdef OPTION_DEBUG_CONSOLE
#include "kernel/debug-console.h"
#endif
#include "kernel-md/smp.h"
#include "kernel-md/interrupts.h"
#include <array>

namespace kdb
{
    namespace
    {
        util::List<detail::KDBCommand> commands;

        constexpr size_t MaxLineLength = 128;
        constexpr size_t MaxArguments = 16;

        int IsActive = 0;
    } // unnamed namespace

    namespace detail
    {
        namespace
        {
            kdb_arg_type_t ResolveArgumentType(const char* q)
            {
                switch (q[0]) {
                    case 's':
                        return T_STRING;
                    case 'i':
                        return T_NUMBER;
                    case 'b':
                        return T_BOOL;
                    default:
                        return T_INVALID;
                }
            }

            template<typename Commands, typename Args>
            KDBCommand* FindCommand(Commands& commands, Args& arg)
            {
                for (auto& cmd : commands) {
                    if (strcmp(arg[0].a_u.u_string, cmd.cmd_command) != 0)
                        continue;

                    return &cmd;
                }
                return nullptr;
            }

            template<typename Args>
            bool ValidateArguments(const KDBCommand& kcmd, int num_arg, Args& arg)
            {
                if (kcmd.cmd_args == nullptr) {
                    if (num_arg != 1) {
                        kprintf("command takes no arguments, yet %d given\n", num_arg - 1);
                        return false;
                    }
                    return true;
                }

                /*
                 * Arguments are either 'type:description' or '[type:description]' - this
                 * is enforced by kdb_register_command() so we can be much less strict here.
                 */
                int cur_arg = 1;
                for (const char* a = kcmd.cmd_args; *a != '\0'; /* nothing */) {
                    auto ka = &arg[cur_arg];
                    int optional = 0;

                    const char* next = strchr(a, ' ');
                    if (next == nullptr)
                        next = strchr(a, '\0');
                    if (*a == '[') {
                        a++; /* skip [ */
                        optional++;
                        next = strchr(a, ']') + 1; /* skip ] */
                    }

                    const char* argname = a + 2; /* skip type: */
                    if (cur_arg >= num_arg) {
                        /* We need more arguments than there are */
                        if (optional) {
                            /* Not a problem; this argument is optional (and any remaining will be,
                             * too!) */
                            break;
                        }

                        kprintf(
                            "Insufficient arguments; expecting '%s'\n",
                            argname); /* XXX stop after end */
                        return false;
                    }

                    auto t = detail::ResolveArgumentType(a);
                    const char* s = ka->a_u.u_string;
                    switch (t) {
                        case T_NUMBER: {
                            char* ptr;
                            unsigned long u = strtoul(s, &ptr, 0);
                            if (*ptr != '\0') {
                                kprintf(
                                    "Argument '%s' is not numeric (got '%s')\n",
                                    argname /* XXX stop after end */, s);
                                return false;
                            }
                            ka->a_type = T_NUMBER;
                            ka->a_u.u_value = u;
                            break;
                        }
                        case T_BOOL: {
                            int v = 0;
                            if (strcmp(s, "1") == 0 || strcmp(s, "on") == 0 ||
                                strcmp(s, "yes") == 0 || strcmp(s, "true") == 0) {
                                v = 1;
                            } else if (
                                strcmp(s, "0") == 0 || strcmp(s, "off") == 0 ||
                                strcmp(s, "no") == 0 || strcmp(s, "false") == 0) {
                                v = 0;
                            } else {
                                kprintf(
                                    "Argument '%s' is not a boolean (got '%s')\n",
                                    argname /* XXX stop after end */, s);
                                return false;
                            }
                            ka->a_type = T_BOOL;
                            ka->a_u.u_value = v;
                            break;
                        }
                        case T_STRING:
                        default:
                            /* Nothing to do; it's already this */
                            break;
                    }

                    /* Go to the next argument */
                    a = next;
                    while (*a == ' ')
                        a++;
                    cur_arg++;
                }

                if (num_arg > cur_arg) {
                    kprintf(
                        "Too many arguments (command takes %d argument(s), yet %d given)\n",
                        cur_arg - 1, num_arg - 1);
                    return false;
                }

                return true;
            }

        } // unnamed namespace

        void AddCommand(KDBCommand& cmd)
        {
            KASSERT(cmd.cmd_command != nullptr, "bad command");

            if (cmd.cmd_args != NULL) {
                /*
                 * Arguments must be either 'type:description' or '[type:description]' -
                 * check here if that matches up and refuse to add the command otherwise.
                 *
                 * Another requirement is that there cannot be any required arguments after
                 * optional ones.
                 */
                int have_opt = 0;
                for (const char* a = cmd.cmd_args; *a != '\0'; /* nothing */) {
                    /* Locate the terminator of this argument */
                    const char* end = strchr(a, ' ');
                    if (end == NULL)
                        end = strchr(a, '\0');

                    /* Parse optional/required argument */
                    if (*a == '[') {
                        const char* p = strchr(a, ']');
                        if (p == NULL || p >= end) {
                            panic(
                                "KDB: command '%s' rejected, end of argument not found (%s)\n",
                                cmd.cmd_command, a);
                        }
                        a++;         /* skip [ */
                        end = p + 1; /* skip ] */
                        have_opt++;
                    } else {
                        if (have_opt) {
                            panic(
                                "KDB: command '%s' rejected, required arguments after optional "
                                "ones (%s)\n",
                                cmd.cmd_command, a);
                        }
                    }

                    const char* split = strchr(a, ':');
                    if (split == NULL || split >= end) {
                        panic(
                            "KDB: command '%s' rejected, type/description split not found\n",
                            cmd.cmd_command);
                    }

                    if (split - a != 1) {
                        panic(
                            "KDB: command '%s' rejected, expected a single type char, got '%s'\n",
                            cmd.cmd_command, a);
                    }

                    if (ResolveArgumentType(a) == T_INVALID) {
                        panic(
                            "KDB: command '%s' rejected, invalid type '%s'\n", cmd.cmd_command, a);
                    }

                    /* Skip this command and go to the next, ignoring extra spaces */
                    a = end;
                    while (*a == ' ')
                        a++;
                }
            }
            commands.push_back(cmd);
        }

        template<typename Args>
        int DissectArguments(char* line, Args& arg)
        {
            int num_arg = 0;
            char* cur_line = line;
            while (1) {
                arg[num_arg].a_type = T_STRING;
                arg[num_arg].a_u.u_string = cur_line;
                num_arg++;
                if (num_arg >= arg.size()) {
                    kprintf("ignoring excessive arguments\n");
                    break;
                }
                char* argend = strchr(cur_line, ' ');
                if (argend == NULL)
                    break;
                /* isolate the argument here */
                *argend++ = '\0';
                while (*argend == ' ')
                    argend++;
                cur_line = argend;
            }
            return num_arg;
        }

    } // namespace detail

    static int kdb_get_line(char* line, int max_len)
    {
#ifdef OPTION_DEBUG_CONSOLE
        /* Poor man's console input... */
        int cur_pos = 0;
        while (1) {
            int ch = debugcon_getch();
            switch (ch) {
                case 0:
                    continue;
                case 8:    /* backspace */
                case 0x7f: /* del */
                    if (cur_pos > 0) {
                        console_putchar(8);
                        console_putchar(' ');
                        console_putchar(8);
                        cur_pos--;
                    }
                    continue;
                case '\n':
                case '\r':
                    console_putchar('\n');
                    line[cur_pos] = '\0';
                    return cur_pos;
                default:
                    if (cur_pos < max_len) {
                        console_putchar(ch);
                        line[cur_pos] = ch;
                        cur_pos++;
                    }
                    break;
            }
        }

        /* NOTREACHED */
#else
        size_t len = max_len;
        auto result = device_read(console_tty, line, &len, 0);
        KASSERT(result.IsSuccess(), "tty read failed with error %i", result.AsStatusCode());
        KASSERT(len > 0, "tty read returned without data");
        line[len] = '\0';
        return len;
#endif
    }

    void Enter(const char* why)
    {
        if (IsActive++ > 0)
            return;

        /* Kill interrupts */
        int ints = md::interrupts::SaveAndDisable();

        // XXX We can't recover from this - so we can't ever leave the debugger...
        smp::PanicOthers();

        /* Redirect console to ourselves */
        Device* old_console_tty = console_tty;
        console_tty = NULL;

        kprintf("kdb_enter(): %s\n", why);

        /* loop for commands */
        while (IsActive) {
            kprintf("kdb> ");
            char line[MaxLineLength + 1];
            int len = kdb_get_line(line, sizeof(line));

            /* Kill trailing newline */
            if (len > 0 && line[len - 1] == '\n')
                line[len - 1] = '\0';

            /* Dissect the line; initially, we parse all arguments as strings */
            std::array<Argument, MaxArguments> arg;
            unsigned int num_arg = detail::DissectArguments(line, arg);

            /* Locate the command */
            const auto kcmd = detail::FindCommand(commands, arg);
            if (kcmd == nullptr) {
                kprintf("unknown command - try 'help'\n");
                continue;
            }

            if (detail::ValidateArguments(*kcmd, num_arg, arg))
                kcmd->cmd_func(num_arg, &arg[0]);
        }

        /* Stop console redirection and restore interrupts */
        console_tty = old_console_tty;
        md::interrupts::Restore(ints);
    }

    void OnPanic() { Enter("panic"); }

} // namespace kdb

namespace
{
    const kdb::RegisterCommand kdbHelp("help", "Displays this help", [](int, const kdb::Argument*) {
        /*
         * First step is to determine the maximum command and argument lengths for
         * pretty printing
         */
        int max_cmd_len = 0, max_args_len = 0;
        for (auto& cmd : kdb::commands) {
            if (strlen(cmd.cmd_command) > max_cmd_len)
                max_cmd_len = strlen(cmd.cmd_command);
            if (cmd.cmd_args != nullptr && strlen(cmd.cmd_args) > max_args_len)
                max_args_len = strlen(cmd.cmd_args);
        }

        /* Now off to print! */
        for (auto& cmd : kdb::commands) {
            kprintf("%s", cmd.cmd_command);
            kprintf(" %s", (cmd.cmd_args != NULL) ? cmd.cmd_args : "");
            for (int n =
                     (cmd.cmd_args != NULL ? strlen(cmd.cmd_args) : 0) + strlen(cmd.cmd_command);
                 n < (max_cmd_len + 1 + max_cmd_len + 1); n++)
                kprintf(" ");
            kprintf("%s\n", cmd.cmd_help);
        }
    });

    const kdb::RegisterCommand kdbExit(
        "exit", "Leaves the debugger", [](int, const kdb::Argument*) { kdb::IsActive = 0; });

} // unnamed namespace

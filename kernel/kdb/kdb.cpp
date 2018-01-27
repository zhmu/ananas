#include <ananas/error.h>
#include "kernel/console.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/trace.h"
#include "options.h"
#ifdef OPTION_DEBUG_CONSOLE
#include "kernel/debug-console.h"
#endif
#ifdef OPTION_SMP
#include "kernel/x86/smp.h"
#endif
#include "kernel-md/interrupts.h"

TRACE_SETUP;

static util::List<KDBCommand> kdb_commands;

#define KDB_MAX_LINE 128
#define KDB_MAX_ARGS 16

static int kdb_active = 0;
Thread* kdb_curthread = nullptr;

static kdb_arg_type_t
kdb_resolve_argtype(const char* q)
{
	switch(q[0]) {
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

errorcode_t
kdb_register_command(KDBCommand& cmd)
{
	if(cmd.cmd_command == nullptr) {
		kprintf("kdb_register_command: BAD %p, ", &cmd.cmd_command);
		kprintf("%p, '%s'\n", &cmd, cmd.cmd_command);
	}

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
					kprintf("KDB: command '%s' rejected, end of argument not found (%s)\n", cmd.cmd_command, a);
					return ANANAS_ERROR(BAD_TYPE);
				}
				a++; /* skip [ */
				end = p + 1; /* skip ] */
				have_opt++;
			} else {
				if (have_opt) {
					kprintf("KDB: command '%s' rejected, required arguments after optional ones (%s)\n", cmd.cmd_command, a);
					return ANANAS_ERROR(BAD_TYPE);
				}
			}

			const char* split = strchr(a, ':');
			if (split == NULL || split >= end) {
				kprintf("KDB: command '%s' rejected, type/description split not found\n", cmd.cmd_command);
				return ANANAS_ERROR(BAD_TYPE);
			}

			if (split - a != 1) {
				kprintf("KDB: command '%s' rejected, expected a single type char, got '%s'\n", cmd.cmd_command, a);
				return ANANAS_ERROR(BAD_TYPE);
			}

			if (kdb_resolve_argtype(a) == T_INVALID) {
				kprintf("KDB: command '%s' rejected, invalid type '%s'\n", cmd.cmd_command, a);
				return ANANAS_ERROR(BAD_TYPE);
			}

			/* Skip this command and go to the next, ignoring extra spaces */
			a = end;
			while(*a == ' ')
				a++;
		}
	}
	kdb_commands.push_back(cmd);
	return ananas_success();
}

KDB_COMMAND(help, NULL, "Displays this help")
{
	/*
	 * First step is to determine the maximum command and argument lengths for
	 * pretty printing
	 */
	int max_cmd_len = 0, max_args_len = 0;
	for(auto& cmd: kdb_commands) {
		if (strlen(cmd.cmd_command) > max_cmd_len)
			max_cmd_len = strlen(cmd.cmd_command);
		if (cmd.cmd_args != nullptr && strlen(cmd.cmd_args) > max_args_len)
			max_args_len = strlen(cmd.cmd_args);
	}

	/* Now off to print! */
	for(auto& cmd: kdb_commands) {
		kprintf("%s", cmd.cmd_command);
		kprintf(" %s", (cmd.cmd_args != NULL) ? cmd.cmd_args : "");
		for (int n = (cmd.cmd_args != NULL ? strlen(cmd.cmd_args) : 0) + strlen(cmd.cmd_command); n < (max_cmd_len + 1 + max_cmd_len + 1); n++)
			kprintf(" ");
		kprintf("%s\n", cmd.cmd_help);
	}
}

KDB_COMMAND(exit, NULL, "Leaves the debugger")
{
	kdb_active = 0;
}

static errorcode_t
kdb_init()
{
	/* XXX We should sort the entries here */
	return ananas_success();
}

INIT_FUNCTION(kdb_init, SUBSYSTEM_KDB, ORDER_LAST);

static int
kdb_get_line(char* line, int max_len)
{
#ifdef OPTION_DEBUG_CONSOLE
	/* Poor man's console input... */
	int cur_pos = 0;
	while(1) {
		int ch = debugcon_getch();
		switch(ch) {
			case 0:
				continue;
			case 8: /* backspace */
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
	size_t len = KDB_MAX_LINE;
	errorcode_t err = device_read(console_tty, line, &len, 0);
	KASSERT(ananas_is_success(err), "tty read failed with error %i", err);
	KASSERT(len > 0, "tty read returned without data");
	line[len] = '\0';
	return len;
#endif
}

void
kdb_enter(const char* why)
{
	if (kdb_active++ > 0)
		return;

	/* Kill interrupts */
	int ints = md_interrupts_save_and_disable();

#ifdef OPTION_SMP
	/* XXX We can't recover from this! */
	smp_panic_others();
#endif

	/* Redirect console to ourselves */
	Ananas::Device* old_console_tty = console_tty;
	console_tty = NULL;

	kprintf("kdb_enter(): %s\n", why);

	/* loop for commands */
	while(kdb_active) {
		kprintf("kdb> ");
		char line[KDB_MAX_LINE + 1];
		int len = kdb_get_line(line, sizeof(line));

		/* Kill trailing newline */
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		/* Dissect the line; initially, we parse all arguments as strings */
		char* cur_line = line;
		unsigned int num_arg = 0;
		struct KDB_ARG arg[KDB_MAX_ARGS];
		while(1) {
			arg[num_arg].a_type = T_STRING;
			arg[num_arg].a_u.u_string = cur_line;
			num_arg++;
			if (num_arg >= KDB_MAX_ARGS) {
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

		/* Locate the command */
		KDBCommand* kcmd = nullptr;
		for(auto& cmd: kdb_commands) {
			if (strcmp(arg[0].a_u.u_string, cmd.cmd_command) != 0)
				continue;
			kprintf("[%s][%s][%s][%p]\n", cmd.cmd_command, cmd.cmd_args, cmd.cmd_help, cmd.cmd_func);

			kcmd = &cmd;
			break;
		}
		if (kcmd == nullptr) {
			kprintf("unknown command - try 'help'\n");
			continue;
		}

		/* We know which command it is; now we have to process the arguments, if any */
		if (kcmd->cmd_args != NULL) {
			/*
			 * Arguments are either 'type:description' or '[type:description]' - this
			 * is enforced by kdb_register_command() so we can be much less strict here.
			 */
			int cur_arg = 1;
			for (const char* a = kcmd->cmd_args; *a != '\0' && kcmd != NULL; /* nothing */) {
				struct KDB_ARG* ka = &arg[cur_arg];
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
						/* Not a problem; this argument is optional (and any remaining will be, too!) */
						break;
					}

					kprintf("Insufficient arguments; expecting '%s'\n", argname); /* XXX stop after end */
					kcmd = NULL;
					break;
				}

				kdb_arg_type_t t = kdb_resolve_argtype(a);
				const char* s = ka->a_u.u_string;
				switch(t) {
					case T_NUMBER: {
						char* ptr;
						unsigned long u = strtoul(s, &ptr, 0);
						if (*ptr != '\0') {
							kprintf("Argument '%s' is not numeric (got '%s')\n", argname /* XXX stop after end */, s);
							kcmd = NULL;
						}
						ka->a_type = T_NUMBER;
						ka->a_u.u_value = u;
						break;
					}
					case T_BOOL: {
						int v = 0;
						if (strcmp(s, "1") == 0 || strcmp(s, "on") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "true") == 0) {
							v = 1;
						} else if (strcmp(s, "0") == 0 || strcmp(s, "off") == 0 || strcmp(s, "no") == 0 || strcmp(s, "false") == 0) {
							v = 0;
						} else {
							kprintf("Argument '%s' is not a boolean (got '%s')\n", argname /* XXX stop after end */, s);
							kcmd = NULL;
						}
						ka->a_type = T_BOOL;
						ka->a_u.u_value = v;
						break;
					}
					case T_STRING:
						/* Nothing to do; it's already this */
						break;
					default:
						/* Can't get here */
						break;
				}

				/* Go to the next argument */
				a = next;
				while (*a == ' ')
					a++;
				cur_arg++;
			}

			if (num_arg > cur_arg) {
				kprintf("Too many arguments (command takes %d argument(s), yet %d given)\n", cur_arg - 1, num_arg - 1);
				kcmd = NULL;
			}
		} else {
			if (num_arg != 1) {
				kprintf("command takes no arguments, yet %d given\n", num_arg - 1);
				continue;
			}
		}

		if (kcmd != NULL) /* may be reset to ease error handling */
			kcmd->cmd_func(num_arg, arg);
	}

	/* Stop console redirection and restore interrupts */
	console_tty = old_console_tty;
	md_interrupts_restore(ints);
}

void
kdb_panic()
{
	kdb_curthread = PCPU_GET(curthread);
	kdb_enter("panic");
}

/* vim:set ts=2 sw=2: */

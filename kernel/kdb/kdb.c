#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/console.h>
#include <ananas/pcpu.h>

#define KDB_MAX_LINE 128
#define KDB_MAX_ARGS 16

static struct THREAD kdb_thread;
static const char* kdb_why = NULL;
static int kdb_active = 0;

typedef void kdb_func_t(int num_args, char** arg);

extern kdb_func_t kdb_cmd_help;
extern kdb_func_t kdb_cmd_exit;
extern kdb_func_t kdb_cmd_threads;
extern kdb_func_t kdb_cmd_bio;
extern kdb_func_t kdb_cmd_bootinfo;

struct KDB_COMMAND {
	const char* cmd;
	const char* descr;
	kdb_func_t* func;
} kdb_commands[] = {
	{ "help", "Lists all commands", &kdb_cmd_help },
	{ "exit", "Leave the debugger", &kdb_cmd_exit },
	{ "threads", "Display thread information", &kdb_cmd_threads },
	{ "bio", "Display BIO information", &kdb_cmd_bio },
	{ "bootinfo", "Display bootinfo", &kdb_cmd_bootinfo },
	{ NULL, NULL, NULL }
};

void
kdb_cmd_help(int num_args, char** arg)
{
	for (struct KDB_COMMAND* cmd = kdb_commands; cmd->cmd != NULL; cmd++) {
		kprintf("%s\t\t%s\n",
		 cmd->cmd, cmd->descr);
	}
}

void
kdb_cmd_exit(int num_args, char** arg)
{
	kdb_active = 0;
	thread_suspend(&kdb_thread);
	scheduler_activate();
	schedule();
	/* NOTREACHED */
}

static void
kdb_func()
{
	char line[KDB_MAX_LINE + 1];
	char* arg[KDB_MAX_ARGS];

	/* disable the scheduler; this ensures we remain in a single place */
	scheduler_deactivate();

	kprintf("kdb_enter(): %s\n", kdb_why);

	/* loop for commands */
	while(1) {
		kprintf("kdb> ");
		size_t len = device_read(console_tty, line, KDB_MAX_LINE, 0);
		KASSERT(len > 0, "tty read returned without data");
		line[len] = '\0';

		/* Kill trailing newline */
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		/* Dissect the line */
		const char* cur_line = line;
		unsigned int cur_arg = 0;
		while(1) {
			arg[cur_arg++] = cur_line;
			KASSERT(cur_arg < KDB_MAX_ARGS, "argument count exceeded");
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
		struct KDB_COMMAND* cmd = kdb_commands;
		for (; cmd->cmd != NULL; cmd++) {
			if (strcmp(arg[0], cmd->cmd) != 0)
				continue;

			/* Found command, launch it */
			cmd->func(cur_arg, arg);
			break;
		}

		if (cmd->cmd == NULL)
			kprintf("unknown command - try 'help'\n");
	}
}

void
kdb_init()
{
	thread_init(&kdb_thread, NULL);
}

void
kdb_enter(const char* why)
{
	if (kdb_active++ > 0)
		return;

	kdb_why = why;

	/* Force our thread to restart and reset the entry point */
	md_thread_setkthread(&kdb_thread, kdb_func);
	thread_resume(&kdb_thread);
}

/* vim:set ts=2 sw=2: */

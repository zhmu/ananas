#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/console.h>
#include <ananas/error.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <machine/param.h> /* for PAGE_SIZE */

#define KDB_MAX_LINE 128
#define KDB_MAX_ARGS 16

static struct THREAD kdb_thread;
static const char* kdb_why = NULL;
static int kdb_active = 0;

typedef void kdb_func_t(int num_args, char** arg);

extern kdb_func_t kdb_cmd_help;
extern kdb_func_t kdb_cmd_exit;
extern kdb_func_t kdb_cmd_threads;
extern kdb_func_t kdb_cmd_thread;
extern kdb_func_t kdb_cmd_bio;
extern kdb_func_t kdb_cmd_bootinfo;
extern kdb_func_t kdb_cmd_memory;
extern kdb_func_t kdb_cmd_handle;
extern kdb_func_t kdb_cmd_vfs;
extern kdb_func_t kdb_cmd_devices;
extern kdb_func_t kdb_cmd_irq;

struct KDB_COMMAND {
	const char* cmd;
	const char* descr;
	kdb_func_t* func;
} kdb_commands[] = {
	{ "help", "Lists all commands", &kdb_cmd_help },
	{ "exit", "Leave the debugger", &kdb_cmd_exit },
	{ "threads", "Display list of threads", &kdb_cmd_threads },
	{ "thread", "Display specific thread information", &kdb_cmd_thread },
	{ "bio", "Display BIO information", &kdb_cmd_bio },
	{ "bootinfo", "Display bootinfo", &kdb_cmd_bootinfo },
	{ "memory", "Display memory information", &kdb_cmd_memory },
	{ "handle", "Display specific handle information", &kdb_cmd_handle },
	{ "vfs", "Display filesystem information", &kdb_cmd_vfs },
	{ "devices", "Display devices list", &kdb_cmd_devices },
	{ "irq", "Display IRQ list", &kdb_cmd_irq },
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
	/* NOTREACHED */
}

static void
kdb_func(void* ptr)
{
	char line[KDB_MAX_LINE + 1];
	char* arg[KDB_MAX_ARGS];

	/* disable the scheduler; this ensures we remain in a single place */
	scheduler_deactivate();

	kprintf("kdb_enter(): %s\n", kdb_why);

	/* loop for commands */
	while(1) {
		kprintf("kdb> ");
		size_t len = KDB_MAX_LINE;
		errorcode_t err = device_read(console_tty, line, &len, 0);
		KASSERT(err == ANANAS_ERROR_NONE, "tty read failed with error %i", err);
		KASSERT(len > 0, "tty read returned without data");
		line[len] = '\0';

		/* Kill trailing newline */
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		/* Dissect the line */
		char* cur_line = line;
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
	thread_set_args(&kdb_thread, "[kdb]\0\0", PAGE_SIZE);
}

void
kdb_enter(const char* why)
{
	if (kdb_active++ > 0)
		return;

	kdb_why = why;

	/* Force our thread to restart and reset the entry point */
	md_thread_setkthread(&kdb_thread, kdb_func, NULL);
	thread_resume(&kdb_thread);
}

/* vim:set ts=2 sw=2: */

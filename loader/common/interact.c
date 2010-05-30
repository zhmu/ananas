#include <stdio.h>
#include <loader/diskio.h>
#include <loader/platform.h>
#include <loader/vfs.h>
#include <loader/elf.h>
#include <string.h>

#define MAX_LINE_SIZE 128
#define MAX_ARGS 16

typedef void (command_t)(int num_args, char** arg);

static command_t cmd_help;
static command_t cmd_ls;
static command_t cmd_cat;
static command_t cmd_load;
static command_t cmd_exec;
static command_t cmd_boot;
static command_t cmd_reboot;
static command_t cmd_autoboot;

static uint64_t kernel_entry = 0;

struct COMMAND {
	const char* cmd;
 	command_t* func;
	const char* descr;
} commands[] = {
	{ "?",        &cmd_help,     "This help" },
	{ "ls",       &cmd_ls,       "List files" },
	{ "cat",      &cmd_cat,      "Display file contents" },
	{ "load",     &cmd_load,     "Load a kernel" },
	{ "exec",     &cmd_exec,     "Execute loaded kernel" },
	{ "boot",     &cmd_boot,     "Load and execute a kernel" },
	{ "reboot",   &cmd_reboot,   "Reboot" },
	{ "autoboot", &cmd_autoboot, "Find a kernel and launch it" },
	{ NULL,     NULL,        NULL }
};

static const char* kernels[] = {
	"kernel",
	NULL
};


static void
cmd_help(int num_args, char** arg)
{
	for(struct COMMAND* cmd = commands; cmd->cmd != NULL; cmd++) {
		printf("%s\n %s\n", cmd->cmd, cmd->descr);
	}
}

static void
cmd_ls(int num_args, char** arg)
{
	char* path = "/";
	if (num_args > 1)
		path = arg[1];
	if (!vfs_open(path)) {
		printf("cannot open '%s'\n", path);
		return;
	}
	const char* entry;
	while((entry = vfs_readdir()) != NULL) {
		printf("'%s'\n", entry);
	}
	vfs_close();
}

static void
cmd_cat(int num_args, char** arg)
{	
	char buf[512];

	if (num_args != 2) {
		printf("need an argument\n");
		return;
	}
	if (!vfs_open(arg[1])) {
		printf("cannot open '%s'\n", arg[1]);
		return;
	}
	while(1) {
		size_t len = vfs_read(buf, sizeof(buf));
		if (len == 0)
			break;
		buf[len] = '\0';
		printf("%s", buf);
	}
	vfs_close();
}

static void
cmd_load(int num_args, char** arg)
{	
	if (num_args != 2) {
		printf("need an argument\n");
		return;
	}
	if (!vfs_open(arg[1])) {
		printf("cannot open '%s'\n", arg[1]);
		return;
	}
	if (!elf_load(&kernel_entry)) {
		printf("couldn't load kernel\n");
		vfs_close();
		return;
	}
	vfs_close();

	printf("loaded successfully, entry point is 0x%x\n", kernel_entry & 0xffffffff);
}

static void
cmd_exec(int num_args, char** arg)
{	
	/* TODO: arguments */
	if (kernel_entry == 0) {
		printf("no kernel loaded\n");
		return;
	}

	platform_cleanup();

	typedef void kentry(void);
	uint32_t entry32 = (kernel_entry & 0x0fffffff);
	((kentry*)entry32)();
}

static void
cmd_boot(int num_args, char** arg)
{	
	cmd_load(num_args, arg);
	if (kernel_entry == 0) {
		/* Must have gone wrong... */
		return;
	}
	cmd_exec(num_args - 1, arg + 1);
}

static void
cmd_reboot(int num_args, char** arg)
{	
	platform_reboot();
}

static void
cmd_autoboot(int num_args, char** arg)
{	
	/*
	 * Try our list of kernels.
	 */
	for (const char** kernel = kernels; *kernel != NULL; kernel++) {
		printf("Trying '%s'...", *kernel);
		const char* kernelfile[] = { NULL, *kernel };
		cmd_load(sizeof(kernelfile) / sizeof(char*), (char**)kernelfile);
		if (kernel_entry == 0)
			continue;
		
		cmd_exec(num_args, arg);
	}
}

void
interact()
{
	char line[MAX_LINE_SIZE + 1];
	char* args[MAX_ARGS];

	while (1) {
		/* Assemble an input line */
		printf("loader> ");
		int len = 0;
		while(1) {
			int ch = platform_getch();
			if (ch == '\r') /* return */
				break;
			if (ch == '\b') /* backspace */ {
				if (len > 0) {
					len--;
					printf("\b \b");
				}
				continue;
			}
			if (ch == 21 /* ctrl-u */) {
				while(len > 0) {
					printf("\b \b");
					len--;
				}
				continue;
			}
			if (ch == 23 /* ctrl-w */) {
				while(len > 0) {
					if (line[len - 1] == ' ')
						break;
					printf("\b \b");
					len--;
				}
				while(len > 0 && line[len - 1] == ' ') {
					printf("\b \b");
					len--;
				}
				continue;
			}
			if (len >= MAX_LINE_SIZE)
				continue;
			line[len++] = (char)ch;
			platform_putch(ch);
		}
		platform_putch('\n');
		line[len] = '\0';

		/*
		 * Dissect the input line into arguments.
		 */
		char* ptr = line;
		int num_args = 0;
		while (num_args < MAX_ARGS) {
			args[num_args++] = ptr;
			char* tmp = strchr(ptr, ' ');
			if (tmp == NULL)
				break;
			*tmp++ = '\0';
			while(*tmp == ' ') tmp++;
			ptr = tmp;
		}
		args[num_args] = NULL;

		/* Find the command */
		struct COMMAND* cmd = commands;
		while(cmd->cmd != NULL) {
			if (strcmp(args[0], cmd->cmd) == 0)
				break;
			cmd++;
		}
		if (cmd->cmd == NULL) {
			printf("unsupported command - use ? for help\n");
			continue;
		}

		cmd->func(num_args, args);
	}
}

/* vim:set ts=2 sw=2: */

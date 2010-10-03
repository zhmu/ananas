#include <ananas/types.h>
#include <ananas/bootinfo.h>
#include <loader/diskio.h>
#include <loader/lib.h>
#include <loader/platform.h>
#include <loader/vfs.h>
#include <loader/elf.h>
#include <loader/ramdisk.h>

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
static command_t cmd_ramdisk;

static struct BOOTINFO bootinfo = { 0 };
struct LOADER_ELF_INFO elf_info = { 0 };

struct COMMAND {
	const char* cmd;
 	command_t* func;
	const char* descr;
} commands[] = {
	{ "?",        &cmd_help,     "This help" },
	{ "ls",       &cmd_ls,       "List files" },
	{ "cat",      &cmd_cat,      "Display file contents" },
	{ "load",     &cmd_load,     "Load a kernel" },
#ifdef RAMDISK
	{ "ramdisk",  &cmd_ramdisk,  "Load a ramdisk" },
#endif
	{ "exec",     &cmd_exec,     "Execute loaded kernel" },
	{ "boot",     &cmd_boot,     "Load and execute a kernel" },
	{ "reboot",   &cmd_reboot,   "Reboot" },
	{ "autoboot", &cmd_autoboot, "Find a kernel and launch it" },
	{ NULL,       NULL,          NULL }
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
	if (!elf_load(&elf_info)) {
		printf("couldn't load kernel\n");
		vfs_close();
		return;
	}
	vfs_close();

	memset(&bootinfo, 0, sizeof(struct BOOTINFO));
	bootinfo.bi_size        = sizeof(struct BOOTINFO);
	bootinfo.bi_kernel_addr = elf_info.elf_start_addr;
	bootinfo.bi_kernel_size = elf_info.elf_end_addr - elf_info.elf_start_addr;

	printf("loaded successfully at 0x%x-0x%x (%u bit kernel)\n",
	 bootinfo.bi_kernel_addr,
	 bootinfo.bi_kernel_addr + bootinfo.bi_kernel_size,
	 elf_info.elf_bits);
}

static void
cmd_exec(int num_args, char** arg)
{	
	/* TODO: arguments */
	if (elf_info.elf_bits == 0) {
		printf("no kernel loaded\n");
		return;
	}

	platform_cleanup();

	platform_exec(&elf_info, &bootinfo);
}

static void
cmd_boot(int num_args, char** arg)
{	
	cmd_load(num_args, arg);
	if (elf_info.elf_bits == 0) {
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
		if (elf_info.elf_bits == 0)
			continue;
		
		cmd_exec(num_args, arg);
	}
}

void
interact()
{
	char line[MAX_LINE_SIZE + 1];
	char* args[MAX_ARGS];

	const char* kernelfile[] = { NULL, "kernel" };
	cmd_load(sizeof(kernelfile) / sizeof(char*), (char**)kernelfile);

	const char* ramdiskfile[] = { NULL, "root.fs" };
	cmd_ramdisk(sizeof(ramdiskfile) / sizeof(char*), (char**)ramdiskfile);

	cmd_exec(0, NULL);

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

#ifdef RAMDISK
static void
cmd_ramdisk(int num_args, char** arg)
{	
	if (num_args != 2) {
		printf("need an argument\n");
		return;
	}
	if (bootinfo.bi_kernel_size == 0) {
		printf("a kernel must be loaded first\n");
		return;
	}
	if (!vfs_open(arg[1])) {
		printf("cannot open '%s'\n", arg[1]);
		return;
	}
	struct LOADER_RAMDISK_INFO ram_info;
	ram_info.ram_start = bootinfo.bi_kernel_addr + bootinfo.bi_kernel_size;
	/*
	 * XXX give the kernel some slack - most implementations use some data after the
	 *     kernel for temporary storage while initializing
	 */
	ram_info.ram_start += 64 * 1024;
	if (!ramdisk_load(&ram_info)) {
		printf("couldn't load ramdisk\n");
		vfs_close();
		return;
	}
	vfs_close();
	bootinfo.bi_ramdisk_addr = ram_info.ram_start;
	bootinfo.bi_ramdisk_size = ram_info.ram_size;

	printf("ramdisk loaded successfully at 0x%x-0x%x\n",
	 bootinfo.bi_ramdisk_addr,
	 bootinfo.bi_ramdisk_addr + bootinfo.bi_ramdisk_size);
}
#endif

/* vim:set ts=2 sw=2: */

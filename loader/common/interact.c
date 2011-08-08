#include <ananas/types.h>
#include <loader/diskio.h>
#include <loader/lib.h>
#include <loader/module.h>
#include <loader/platform.h>
#include <loader/vfs.h>
#include <loader/ramdisk.h>

#define MAX_LINE_SIZE 128
#define MAX_ARGS 16
#define AUTOBOOT_TIMEOUT 3

typedef void (command_t)(int num_args, char** arg);

static command_t cmd_help;
static command_t cmd_ls;
static command_t cmd_cat;
static command_t cmd_load;
static command_t cmd_exec;
static command_t cmd_boot;
static command_t cmd_reboot;
static command_t cmd_autoboot;
#ifdef RAMDISK
static command_t cmd_ramdisk;
#endif
static command_t cmd_cache;
static command_t cmd_lsdev;
static command_t cmd_mount;
#ifdef VBE
extern command_t cmd_modes;
extern command_t cmd_setmode;
#endif
static command_t cmd_source;
static command_t cmd_module;
static command_t cmd_modules;

static int are_sourcing = 0;

struct COMMAND {
	const char* cmd;
 	command_t* func;
	const char* descr;
} commands[] = {
	{ "?",        &cmd_help,     "This help" },
	{ "ls",       &cmd_ls,       "List files" },
	{ "lsdev",    &cmd_lsdev,    "List devices" },
	{ "cat",      &cmd_cat,      "Display file contents" },
	{ "load",     &cmd_load,     "Load a kernel" },
	{ "source",   &cmd_source,   "Run a script" },
#ifdef RAMDISK
	{ "ramdisk",  &cmd_ramdisk,  "Load a ramdisk" },
#endif
	{ "exec",     &cmd_exec,     "Execute loaded kernel" },
	{ "boot",     &cmd_boot,     "Load and execute a kernel" },
	{ "reboot",   &cmd_reboot,   "Reboot" },
	{ "autoboot", &cmd_autoboot, "Find a kernel and launch it" },
	{ "cache",    &cmd_cache,    "Display I/O cache stats" },
	{ "mount",    &cmd_mount,    "Mount filesystem" },
#ifdef VBE
	{ "modes",    &cmd_modes,    "Display available screen modes" },
	{ "setmode",  &cmd_setmode,  "Set screen resolution after boot" },
#endif
	{ "module",   &cmd_module,   "Load a module" },
	{ "modules",  &cmd_modules,  "Display loaded modules" },
	{ NULL,       NULL,          NULL }
};

static const char* kernels[] = {
	"kernel",
	NULL
};

static const char* loadercfgs[] = {
	"loader.cfg",
	NULL
};

static void
cmd_help(int num_args, char** arg)
{
	int width = 0;
	/* Calculate the maximum width of a command */
	for(struct COMMAND* cmd = commands; cmd->cmd != NULL; cmd++) {
		int len = strlen(cmd->cmd);
		if (width < len)
			width = len;
	}

	for(struct COMMAND* cmd = commands; cmd->cmd != NULL; cmd++) {
		printf("%s", cmd->cmd);
		for (int i = strlen(cmd->cmd); i < width + 2; i++)
			platform_putch(' ');
		printf("%s\n", cmd->descr);
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
	if (!module_load(MOD_KERNEL)) {
		printf("couldn't load kernel\n");
		vfs_close();
		return;
	}
	vfs_close();

	printf("loaded successfully at 0x%x-0x%x (%u bit kernel",
	 mod_kernel.mod_phys_start_addr, mod_kernel.mod_phys_end_addr,
	 mod_kernel.mod_bits);
	if (mod_kernel.mod_symtab_addr != 0)
		printf(", symbols");
	if (mod_kernel.mod_strtab_addr != 0)
		printf(", strings");
	printf(")\n");
}

static void
cmd_exec(int num_args, char** arg)
{	
	/* TODO: arguments */
	if (mod_kernel.mod_bits == 0) {
		printf("no kernel loaded\n");
		return;
	}

	platform_cleanup();

	struct BOOTINFO* bootinfo;
	module_make_bootinfo(&bootinfo);
	platform_exec(&mod_kernel, bootinfo);
}

static void
cmd_boot(int num_args, char** arg)
{	
	cmd_load(num_args, arg);
	if (mod_kernel.mod_bits == 0) {
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
		if (mod_kernel.mod_bits == 0)
			continue;
		
		cmd_exec(num_args, arg);
	}
}

int
execute(char* line)
{
	char* args[MAX_ARGS];

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
		printf("unsupported command '%s' - use ? for help\n", args[0]);
		return 0;
	}

	cmd->func(num_args, args);
	return 1;
}

static void
source_file()
{
#define MAX_SOURCE_SIZE 1024
	static char* buffer = NULL;
	if (buffer == NULL)
		buffer = platform_get_memory(MAX_SOURCE_SIZE + 1);
	buffer[MAX_SOURCE_SIZE] = '\0';
	size_t len = vfs_read(buffer, MAX_SOURCE_SIZE);
	buffer[len] = '\0';
	vfs_close();
	/* XXX Can't check whether the buffer is fully filled yet; FAT will always
   * return what is requested as it doesn't check file sizes */

	/* Run the commands one by one */
	are_sourcing++;
	char* line = buffer;
	while(line < buffer + len) {
		char* ptr = strchr(line, '\n');
		if (ptr != NULL)
			*ptr++ = '\0';
		if (*line != '\0') /* ignore blanks */
			execute(line);
		if (ptr == NULL)
			break;
		line = ptr;
	}
	are_sourcing--;
}

void
autoboot()
{
	/* Look for a config file we can read */
	for (const char** cfg = loadercfgs; *cfg != NULL; cfg++) {
		if (!vfs_open(*cfg))
			continue;

		/* This file seems to work */
		printf("Launching '%s' in %u seconds...", *cfg, AUTOBOOT_TIMEOUT);
		for (int i = AUTOBOOT_TIMEOUT; i > 0; i--) {
			printf("%u...", i);
			/* Delay in steps of 100ms to give the user a chance to abort */
			for (int j = 0; j < 10; j++) {
				platform_delay(100);
				if (platform_check_key()) {
					(void)platform_getch(); /* eat keystroke */
					printf(" aborting\n");
					return;
				}
			}
		}

		/* Timeout; launch the file. We break in case it returns */
		printf(" 0!\n");
		source_file();
		break;
	}
}

void
interact()
{
	char line[MAX_LINE_SIZE + 1];

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

		execute(line);
	}
}

static void
cmd_cache(int num_args, char** arg)
{
	diskio_stats();
}

#ifdef RAMDISK
static void
cmd_ramdisk(int num_args, char** arg)
{	
	if (num_args != 2) {
		printf("need an argument\n");
		return;
	}
	if (mod_kernel.mod_bits == 0) {
		printf("a kernel must be loaded first\n");
		return;
	}
	if (!vfs_open(arg[1])) {
		printf("cannot open '%s'\n", arg[1]);
		return;
	}
	struct LOADER_RAMDISK_INFO ram_info;
	ram_info.ram_start = mod_kernel.mod_phys_end_addr;
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
}
#endif

static void
cmd_lsdev(int num_args, char** arg)
{
	diskio_lsdev();
}

static void
cmd_mount(int num_args, char** arg)
{
	if (num_args == 1) {
		if (vfs_current_device < 0) {
			printf("nothing is mounted\n");
		} else {
			printf("currently mounted: %s (%s)\n",
			 diskio_get_name(vfs_current_device), vfs_get_current_fstype());
		}
		return;
	}

	if (num_args > 2) {
		printf("need a disk device, or nothing to show currently mounted device\n");
		return;
	}

	int iodevice = diskio_find_disk(arg[1]);
	if (iodevice < 0) {
		printf("device not found\n");
		return;
	}

	const char* type;
	if (!vfs_mount(iodevice, &type)) {
		printf("unable to mount\n");
		return;
	}
	printf("mounted %s (%s)\n", arg[1], type);
}

static void
cmd_source(int num_args, char** arg)
{
	if (are_sourcing) {
		printf("'source' cannot be nested\n");
		return;
	}
	if (num_args != 2) {
		printf("need an argument\n");
		return;
	}

	if (!vfs_open(arg[1])) {
		printf("cannot open source file\n");
		return;
	}

	source_file();
}

static void
cmd_module(int num_args, char** arg)
{	
	if (num_args != 2) {
		printf("need an argument\n");
		return;
	}
	if (!vfs_open(arg[1])) {
		printf("cannot open '%s'\n", arg[1]);
		return;
	}
	if (!module_load(MOD_MODULE)) {
		printf("couldn't load module\n");
		vfs_close();
		return;
	}
	vfs_close();
}

static void
cmd_modules(int num_args, char** arg)
{
	int i = 0;
	for (struct LOADER_MODULE* mod_info = &mod_kernel; mod_info != NULL; mod_info = mod_info->mod_next, i++) {
		printf("%u. %x-%x\n", i, mod_info->mod_phys_start_addr, mod_info->mod_phys_end_addr);
	}
}

/* vim:set ts=2 sw=2: */

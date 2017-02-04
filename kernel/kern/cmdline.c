#include <ananas/bootinfo.h>
#include <ananas/cmdline.h>
#include <ananas/kmem.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/lib.h>

static char* cmdline = NULL;
static int cmdline_len = -1;

void
cmdline_init()
{
	/* Default to no sensible commmandline */
	cmdline_len = 0;

	/* 1 because bi_args_size includes the terminating \0 */
	if (bootinfo->bi_args_size <= 1 || bootinfo->bi_args == 0)
		return;
	size_t bootargs_len = bootinfo->bi_args_size;

	char* bootargs = kmem_map(bootinfo->bi_args, bootargs_len, VM_FLAG_READ);
	if (bootargs[bootinfo->bi_args_size - 1] == '\0') {
		/* Okay, looks sensible - allocate a commandline and copy over */
		cmdline = kmalloc(bootargs_len);
		strcpy(cmdline, bootargs);
		cmdline_len = bootargs_len;

		/*
		 * Transform all spaces to \0's; thisallows cmdline_get_string() to just
		 * return on a match
		 */
		for(int n = 0; n < cmdline_len; n++)
			if (cmdline[n] == ' ')
				cmdline[n] = '\0';
	}
	kmem_unmap(bootargs, bootargs_len);
}

const char*
cmdline_get_string(const char* key)
{
	KASSERT(cmdline_len >= 0, "cmdline not initialized");
	if (cmdline == NULL)
		return NULL;

	const char* cur = cmdline;
	while((cur - cmdline) < cmdline_len) {
		/* Look for separator, = or \0 will do */
		const char* sep = cur;
		while(*sep != '\0' && *sep != '=')
			sep++;

		if (strncmp(cur, key, sep - cur) != 0) {
			/* No match; next one */
			cur = sep + 1;
			continue;
		}

		if (*sep == '=')
			sep++; /* skip the actual separator */
		return sep;
	}

	return NULL; /* not found */
}

/* vim:set ts=2 sw=2: */

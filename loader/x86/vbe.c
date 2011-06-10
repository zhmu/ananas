#include <loader/lib.h>
#include <ananas/bootinfo.h>
#include <loader/platform.h>
#include <loader/x86.h>
#include <loader/vbe.h>
#include <machine/param.h> /* for PAGE_SIZE */

#ifdef VBE

static struct VBE_MODE* vbe_modes = NULL;
static struct VBE_MODE* vbe_launch_mode = NULL;

void
vbe_init()
{
	struct REALMODE_REGS regs;

	/* Ask for VBE 2.0; we mainly care about the framebuffer so we need VBE2+ */
	struct VbeInfoBlock* vbeInfoBlock = (struct VbeInfoBlock*)&rm_buffer;
	vbeInfoBlock->VbeSignature = VBEINFO_SIGNATURE_VBE2;

	/* See if this machine can do VBE; it'd really be a miracle if it wouldn't */
	x86_realmode_init(&regs);
	regs.eax = 0x4f00;			/* vbe: return vbe controller information */
	regs.edi = REALMODE_BUFFER;
	regs.interrupt = 0x10;
	x86_realmode_call(&regs);
	if ((regs.eax & 0xffff) != 0x4f)
		return;
	if (vbeInfoBlock->VbeVersion < 0x200) /* ensure VBE 2.0+ is there */
		return;

	/*
	 * Count the number of valid modes; we need this many entries in our buffer.
	 * Note that we don't yet know at this point whether the mode is actually
	 * available, so we may allocate a few entries too much.
	 */
	uint16_t* vbe_modeptr = (uint16_t*)(((vbeInfoBlock->VideoModePtr >> 16) << 4) | (vbeInfoBlock->VideoModePtr & 0xffff));
	int num_modes = 1; /* terminating 0xffff */
	for (uint16_t* mode = vbe_modeptr; *mode != 0xffff; mode++)
		num_modes++;
	/* XXX Round up to the next page; things get confused otherwise */
	unsigned int vbe_size = sizeof(struct VBE_MODE) * num_modes;
	if (vbe_size % PAGE_SIZE)
		vbe_size = (vbe_size | (PAGE_SIZE - 1)) + 1;
	vbe_modes = platform_get_memory(vbe_size);

	/* Now add all of our modes to the list */
	int mode_idx = 0;
	for (uint16_t* mode = vbe_modeptr; *mode != 0xffff; mode++) {
		x86_realmode_init(&regs);
		regs.eax = 0x4f01;			/* vbe: return vbe mode information */
		regs.ecx = *mode;
		regs.edi = REALMODE_BUFFER + 512; /* Don't overwrite the info block */
		regs.interrupt = 0x10;
		x86_realmode_call(&regs);
		if ((regs.eax & 0xffff) != 0x4f)
			continue;

		/* Skip modes that cannot be set (why did we get it in the first place?!) */
		struct ModeInfoBlock* modeInfoBlock = (struct ModeInfoBlock*)((addr_t)&rm_buffer + 512);
		if ((modeInfoBlock->ModeAttributes & VBE_MODEATTR_SUPPORTED) == 0)
			continue;
		/* Skip anything without a framebuffer too; it'll be useless to us */
		if ((modeInfoBlock->ModeAttributes & VBE_MODEATTR_FRAMEBUFFER) == 0)
			continue;
		/* Skip anything that isn't packed-pixel (palette) or direct color */
		if (modeInfoBlock->MemoryModel != VBE_MEMMODEL_PACKED_PIXEL &&
		    modeInfoBlock->MemoryModel != VBE_MEMMODEL_DIRECTCOLOR)
			continue;

		/* Mode looks sane, add it */
		struct VBE_MODE* vbe_mode = &vbe_modes[mode_idx];
		vbe_mode->mode = *mode;
		vbe_mode->x_res = modeInfoBlock->XResolution;
		vbe_mode->y_res = modeInfoBlock->YResolution;
		vbe_mode->bpp = modeInfoBlock->BitsPerPixel;
		vbe_mode->framebuffer = modeInfoBlock->PhysBasePtr;
		vbe_mode->flags = 0;
		if (modeInfoBlock->MemoryModel == VBE_MEMMODEL_PACKED_PIXEL)
			vbe_mode->flags |= VBEMODE_FLAG_PALETTE;
		mode_idx++;
	}

	/* Add end-of-the-line mode */
	vbe_modes[mode_idx].mode = VBEMODE_NONE;
}

int
vbe_setmode(struct BOOTINFO* bootinfo)
{
	if (vbe_launch_mode == NULL)
		return 1; /* nothing to do */

	struct REALMODE_REGS regs;
	x86_realmode_init(&regs);
	regs.eax = 0x4f02;			/* vbe: set vbe mode */
	regs.ebx = vbe_launch_mode->mode | VBE_SETMODE_FLAT_FRAMEBUFFER;
	regs.interrupt = 0x10;
	x86_realmode_call(&regs);
	if ((regs.eax & 0xffff) != 0x4f)
		return 0;

	/* This worked; inform the kernel of this by using the bootinfo structure */
	bootinfo->bi_video_xres = vbe_launch_mode->x_res;
	bootinfo->bi_video_yres = vbe_launch_mode->y_res;
	bootinfo->bi_video_bpp = vbe_launch_mode->bpp;
	bootinfo->bi_video_framebuffer = vbe_launch_mode->framebuffer;
	return 1;
}

void
cmd_modes(int num_args, char** arg)
{
	if (vbe_modes == NULL) {
		printf("VBE extensions not available\n");
		return;
	}

	for (struct VBE_MODE* vbe_mode = vbe_modes; vbe_mode->mode != VBEMODE_NONE; vbe_mode++) {
		printf("mode %x: %u x %u @ %u bpp\n",
		 vbe_mode->mode, vbe_mode->x_res, vbe_mode->y_res, vbe_mode->bpp);
	}
}

void
cmd_setmode(int num_args, char** arg)
{
	if (num_args > 4) {
		printf("supply at most xresolution, yresolution and bits per pixel\n");
		return;
	}

	char* ptr;
	int xres = 0, yres = 0, bpp = 0;
	if (num_args > 1) {
		xres = strtol(arg[1], &ptr, 10);
		if (*ptr != '\0') {
			printf("invalid x resolution\n");
			return;
		}
	}
	if (num_args > 2) {
		yres = strtol(arg[2], &ptr, 10);
		if (*ptr != '\0') {
			printf("invalid y resolution\n");
			return;
		}
	}
	if (num_args > 3) {
		bpp = strtol(arg[3], &ptr, 10);
		if (*ptr != '\0' || (bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)) {
			printf("invalid bpp; must be 8, 16, 24 or 32\n");
			return;
		}
	}

	/* Scan for the mode through our mode list */
	vbe_launch_mode  = NULL;
	int cur_x = 0, cur_y = 0, cur_bpp = 0;
	for (struct VBE_MODE* vbe_mode = vbe_modes; vbe_mode->mode != VBEMODE_NONE; vbe_mode++) {
		/* Look for a match if field is given, or the best if the given value is 0 */
#define HANDLE_MATCH(x, c, field) \
	if ((x) == 0 && (c)  > vbe_mode->field) continue; \
	if ((x) != 0 && (x) != vbe_mode->field) continue;

		HANDLE_MATCH(xres, cur_x,  x_res);
		HANDLE_MATCH(yres, cur_y,  y_res);
		HANDLE_MATCH(bpp, cur_bpp, bpp);

#undef HANDLE_MATCH

		/* Found a matching or better mode */
		vbe_launch_mode = vbe_mode;
		cur_x = vbe_mode->x_res;
		cur_y = vbe_mode->y_res;
		cur_bpp = vbe_mode->bpp;
	}
	if (vbe_launch_mode == NULL) {
		printf("mode not found - use 'modes' for a list\n");
		return;
	}
	printf("mode %x: %u x %u @ %u will be set\n", vbe_launch_mode->mode, cur_x, cur_y, cur_bpp);
}
#endif /* VBE */

/* vim:set ts=2 sw=2: */

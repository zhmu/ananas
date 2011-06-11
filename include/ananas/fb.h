#ifndef __FB_H__
#define __FB_H__

#include <ananas/handle-options.h>

#define HCTL_FB_GETINFO		(_HCTL_DEVICE_FIRST + 0)	/* Obtain framebuffer information */
struct HCTL_FB_INFO_ARG {
	unsigned int	fb_xres;		/* X resolution */
	unsigned int	fb_yres;		/* Y resolution */
	unsigned int	fb_bpp;			/* Bits per pixel */
};
#define HCTL_FB_CLAIM		(_HCTL_DEVICE_FIRST + 1)	/* Obtain control of frame buffer */
struct HCTL_FB_CLAIM_ARG {
	void*		fb_framebuffer;		/* Pointer to framebuffer */
	size_t		fb_size;		/* Framebuffer length */
};
#define HCTL_FB_RELEASE		(_HCTL_DEVICE_FIRST + 2)	/* Release control of frame buffer */

#endif /* __FB_H__ */

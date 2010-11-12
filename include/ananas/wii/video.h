#ifndef __ANANAS_WII_VIDEO_H__
#define __ANANAS_WII_VIDEO_H__

int   wiivideo_init();
void  wiivideo_get_size(int* height, int* width);
void* wiivideo_get_framebuffer();

#endif /* __ANANAS_WII_VIDEO_H__ */

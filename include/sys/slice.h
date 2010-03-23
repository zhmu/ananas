#include <sys/types.h>
#include <sys/device.h>

#ifndef __SLICE_H__
#define __SLICE_H__

struct DEVICE* slice_create(device_t dev, block_t begin, block_t length);

#endif /* __SLICE_H__ */

#include <ananas/types.h>
#include <ananas/device.h>

#ifndef __SLICE_H__
#define __SLICE_H__

struct DEVICE* slice_create(device_t dev, blocknr_t begin, blocknr_t length);

#endif /* __SLICE_H__ */

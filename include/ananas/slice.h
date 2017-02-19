#include <ananas/types.h>

#ifndef __SLICE_H__
#define __SLICE_H__

namespace Ananas {
class Device;
}

Ananas::Device* slice_create(Ananas::Device* parent, blocknr_t begin, blocknr_t length);

#endif /* __SLICE_H__ */

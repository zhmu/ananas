#ifndef ANANAS_ANKFS_NETWORK_H
#define ANANAS_ANKFS_NETWORK_H

#include <ananas/types.h>

namespace Ananas {
namespace AnkhFS {

class IAnkhSubSystem;

IAnkhSubSystem& GetNetworkSubSystem();

} // namespace AnkhFS
} // namespace Ananas

#endif // ANANAS_ANKFS_NETWORK_H

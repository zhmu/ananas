#ifndef ANANAS_ANKFS_ROOT_H
#define ANANAS_ANKFS_ROOT_H

#include <ananas/types.h>

namespace Ananas {
namespace AnkhFS {

class IAnkhSubSystem;

IAnkhSubSystem& GetRootSubSystem();

} // namespace AnkhFS
} // namespace Ananas

#endif // ANANAS_ANKFS_ROOT_H
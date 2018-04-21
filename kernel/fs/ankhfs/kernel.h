#ifndef ANANAS_ANKFS_KERNEL_H
#define ANANAS_ANKFS_KERNEL_H

#include <ananas/types.h>

namespace Ananas {
namespace AnkhFS {

class IAnkhSubSystem;

IAnkhSubSystem& GetKernelSubSystem();

} // namespace AnkhFS
} // namespace Ananas

#endif // ANANAS_ANKFS_KERNEL_H

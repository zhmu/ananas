#ifndef ANANAS_ANKFS_KERNEL_H
#define ANANAS_ANKFS_KERNEL_H

#include <ananas/types.h>

namespace ankhfs
{
    class IAnkhSubSystem;

    IAnkhSubSystem& GetKernelSubSystem();

} // namespace ankhfs

#endif // ANANAS_ANKFS_KERNEL_H

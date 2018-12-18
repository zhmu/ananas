#ifndef ANANAS_ANKFS_ROOT_H
#define ANANAS_ANKFS_ROOT_H

#include <ananas/types.h>

namespace ankhfs
{
    class IAnkhSubSystem;

    IAnkhSubSystem& GetRootSubSystem();

} // namespace ankhfs

#endif // ANANAS_ANKFS_ROOT_H

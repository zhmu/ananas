#ifndef ANANAS_ANKFS_FILESYSTEM_H
#define ANANAS_ANKFS_FILESYSTEM_H

#include <ananas/types.h>

namespace ankhfs {

class IAnkhSubSystem;

IAnkhSubSystem& GetFileSystemSubSystem();

} // namespace ankhfs

#endif // ANANAS_ANKFS_FILESYSTEM_H

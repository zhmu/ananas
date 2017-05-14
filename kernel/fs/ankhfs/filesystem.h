#ifndef ANANAS_ANKFS_FILESYSTEM_H
#define ANANAS_ANKFS_FILESYSTEM_H

#include <ananas/types.h>

namespace Ananas {
namespace AnkhFS {

class IAnkhSubSystem;

IAnkhSubSystem& GetFileSystemSubSystem();

} // namespace AnkhFS
} // namespace Ananas

#endif // ANANAS_ANKFS_FILESYSTEM_H
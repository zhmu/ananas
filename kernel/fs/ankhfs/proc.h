#ifndef ANANAS_ANKFS_PROC_H
#define ANANAS_ANKFS_PROC_H

#include <ananas/types.h>

namespace Ananas {
namespace AnkhFS {

class IAnkhSubSystem;

IAnkhSubSystem& GetProcSubSystem();

} // namespace AnkhFS
} // namespace Ananas

#endif // ANANAS_ANKFS_PROC_H
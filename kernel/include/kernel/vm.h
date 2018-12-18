#include <ananas/types.h>

#ifndef __VM_H__
#define __VM_H__

#define VM_FLAG_READ (1 << 0)
#define VM_FLAG_WRITE (1 << 1)
#define VM_FLAG_EXECUTE (1 << 2)
#define VM_FLAG_KERNEL (1 << 3)
#define VM_FLAG_USER (1 << 4)
#define VM_FLAG_DEVICE (1 << 5)
#define VM_FLAG_PRIVATE (1 << 6) /* Do not share mapping (used for inodes) */
#define VM_FLAG_FAULT (1 << 7)   /* Force faults */
#define VM_FLAG_MD (1 << 15)     /* Machine dependent mapping */

/* Force a specific mapping to be made */
#define VM_FLAG_FORCEMAP (1 << 16)

#endif /* __VM_H__ */

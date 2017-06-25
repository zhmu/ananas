#ifndef __ANANAS_TYPES_H__
#define __ANANAS_TYPES_H__

#ifndef ASM

#include <machine/_types.h>
#ifdef KERNEL
#include <ananas/stdint.h>
#else
#include <stdint.h>
#endif
#include <ananas/_types/null.h>
#include <ananas/_types/addr.h> /* XXX should this be removed? */
#include <ananas/_types/clock.h>
#include <ananas/_types/pid.h>
#include <ananas/_types/uid.h>
#include <ananas/_types/gid.h>
#include <ananas/_types/mode.h>
#include <ananas/_types/handle.h>
#include <ananas/_types/id.h>
#include <ananas/_types/ino.h>
#include <ananas/_types/off.h>
#include <ananas/_types/nlink.h>
#include <ananas/_types/dev.h>
#include <ananas/_types/dma.h>
#include <ananas/_types/blksize.h>
#include <ananas/_types/blkcnt.h>
#include <ananas/_types/blocknr.h>
#include <ananas/_types/errorcode.h>
#include <ananas/_types/process.h>
#include <ananas/_types/register.h>
#include <ananas/_types/refcount.h>
#include <ananas/_types/spinlock.h>
#include <ananas/_types/size.h>
#include <ananas/_types/ssize.h>
#include <ananas/_types/time.h>
#include <ananas/_types/thread.h>
#include <ananas/_types/vmspace.h>
#include <ananas/_types/vmarea.h>

#endif /* !ASM */

#endif /* __ANANAS_TYPES_H__ */
